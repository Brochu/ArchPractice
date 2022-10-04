void CheckRaytracingSupport()
{
    D3D12_FEATURE_DATA_D3D12_OPTIONS5 options5 = {};
    ThrowIfFailed(m_device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5, &options5, sizeof(options5)));

    if (options5.RaytracingTier < D3D12_RAYTRACING_TIER_1_0)
        throw std::runtime_error("Raytracing not supported on device");
}

// #DXR
struct AccelerationStructureBuffers
{
    ComPtr<id3d12resource> pScratch; // Scratch memory for AS builder
    ComPtr<id3d12resource> pResult; // Where the AS is
    ComPtr<id3d12resource> pInstanceDesc; // Hold the matrices of the instances, only for TLAS
};

ComPtr<id3d12resource> m_bottomLevelAS; // Storage for the bottom Level AS
nv_helpers_dx12::TopLevelASGenerator m_topLevelASGenerator;
AccelerationStructureBuffers m_topLevelASBuffers;
std::vector<std::pair<comptr<id3d12resource>, DirectX::XMMATRIX>> m_instances; // map BLAS to transform

//-----------------------------------------------------------------------------
// Create a vertex buffer for the plane
void D3D12HelloTriangle::CreatePlaneVB()
{
    // Define the geometry for a plane.
    Vertex planeVertices[] = {
        {{-1.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 0
        {{-1.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 1
        {{01.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 2
        {{01.5f, -.8f, 01.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 2
        {{-1.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 1
        {{01.5f, -.8f, -1.5f}, {1.0f, 1.0f, 1.0f, 1.0f}}, // 4
    };
    const UINT planeBufferSize = sizeof(planeVertices);

    // Note: using upload heaps to transfer static data like vert buffers is not
    // recommended. Every time the GPU needs it, the upload heap will be
    // marshalled over. Please read up on Default Heap usage. An upload heap is
    // used here for code simplicity and because there are very few verts to
    // actually transfer.
    CD3DX12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
    CD3DX12_RESOURCE_DESC bufferResource = CD3DX12_RESOURCE_DESC::Buffer(planeBufferSize);
    ThrowIfFailed(m_device->CreateCommittedResource(
        &heapProperty,
        D3D12_HEAP_FLAG_NONE,
        &bufferResource,
        nullptr,
        IID_PPV_ARGS(&m_planeBuffer)
    ));

    // Copy the triangle data to the vertex buffer.
    UINT8 *pVertexDataBegin;
    CD3DX12_RANGE readRange( 0, 0);

    // We do not intend to read from this resource on the CPU.
    ThrowIfFailed(m_planeBuffer->Map( 0, &readRange, reinterpret_cast<void **="">(&pVertexDataBegin)));
    memcpy(pVertexDataBegin, planeVertices, sizeof(planeVertices));
    m_planeBuffer->Unmap(0, nullptr);

    // Initialize the vertex buffer view.
    m_planeBufferView.BufferLocation = m_planeBuffer->GetGPUVirtualAddress();
    m_planeBufferView.StrideInBytes = sizeof(Vertex);
    m_planeBufferView.SizeInBytes = planeBufferSize;
}

//-----------------------------------------------------------------------------
//
// Create a bottom-level acceleration structure based on a list of vertex
// buffers in GPU memory along with their vertex count. The build is then done
// in 3 steps: gathering the geometry, computing the sizes of the required
// buffers, and building the actual AS
//
AccelerationStructureBuffers CreateBottomLevelAS( std::vector<std::pair<comptr<id3d12resource>, uint32_t>> vVertexBuffers)
{
    nv_helpers_dx12::BottomLevelASGenerator bottomLevelAS;
    // Adding all vertex buffers and not transforming their position.
    for (const auto &buffer : vVertexBuffers)
    {
        bottomLevelAS.AddVertexBuffer(buffer.first.Get(), 0, buffer.second, sizeof(Vertex), 0, 0);
    }

    // The AS build requires some scratch space to store temporary information.
    // The amount of scratch memory is dependent on the scene complexity.
    UINT64 scratchSizeInBytes = 0;
    // The final AS also needs to be stored in addition to the existing vertex
    // buffers. It size is also dependent on the scene complexity.
    UINT64 resultSizeInBytes = 0;

    bottomLevelAS.ComputeASBufferSizes(m_device.Get(), false, &scratchSizeInBytes, &resultSizeInBytes);

    // Once the sizes are obtained, the application is responsible for allocating
    // the necessary buffers. Since the entire generation will be done on the GPU,
    // we can directly allocate those on the default heap
    AccelerationStructureBuffers buffers;
    buffers.pScratch = nv_helpers_dx12::CreateBuffer(
            m_device.Get(),
            scratchSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_COMMON,
            nv_helpers_dx12::kDefaultHeapProps
        );
    buffers.pResult = nv_helpers_dx12::CreateBuffer(
            m_device.Get(),
            resultSizeInBytes,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            nv_helpers_dx12::kDefaultHeapProps
        );

    // Build the acceleration structure. Note that this call integrates a barrier
    // on the generated AS, so that it can be used to compute a top-level AS right
    // after this method.
    bottomLevelAS.Generate(m_commandList.Get(), buffers.pScratch.Get(), buffers.pResult.Get(), false, nullptr);
    return buffers;
}

//-----------------------------------------------------------------------------
//
// Create the main acceleration structure that holds all instances of the scene.
// Similarly to the bottom-level AS generation, it is done in 3 steps: gathering
// the instances, computing the memory requirements for the AS, and building the
// AS itself
// Param: instances = pair of bottom level AS and matrix of the instance
//
void CreateTopLevelAS(const std::vector<std::pair<comptr<id3d12resource>, DirectX::XMMATRIX>> &instances)
{
    // Gather all the instances into the builder helper
    for (size_t i = 0; i < instances.size(); i++)
    {
        m_topLevelASGenerator.AddInstance(
                instances[i].first.Get(),
                instances[i].second,
                static_cast<uint>(i),
                static_cast<uint>(0)
            );
    }

    // As for the bottom-level AS, the building the AS requires some scratch space
    // to store temporary data in addition to the actual AS. In the case of the
    // top-level AS, the instance descriptors also need to be stored in GPU
    // memory. This call outputs the memory requirements for each
    // (scratch, results, instance descriptors) so that the application can allocate the
    // corresponding memory
    UINT64 scratchSize, resultSize, instanceDescsSize;
    m_topLevelASGenerator.ComputeASBufferSizes(
            m_device.Get(),
            true,
            &scratchSize,
            &resultSize,
            &instanceDescsSize
        );

    // Create the scratch and result buffers. Since the build is all done on GPU,
    // those can be allocated on the default heap
    m_topLevelASBuffers.pScratch = nv_helpers_dx12::CreateBuffer(
            m_device.Get(),
            scratchSize,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
            nv_helpers_dx12::kDefaultHeapProps
        );
    m_topLevelASBuffers.pResult = nv_helpers_dx12::CreateBuffer(
            m_device.Get(),
            resultSize,
            D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS,
            D3D12_RESOURCE_STATE_RAYTRACING_ACCELERATION_STRUCTURE,
            nv_helpers_dx12::kDefaultHeapProps
        );

    // The buffer describing the instances: ID, shader binding information,
    // matrices ... Those will be copied into the buffer by the helper through
    // mapping, so the buffer has to be allocated on the upload heap.
    m_topLevelASBuffers.pInstanceDesc = nv_helpers_dx12::CreateBuffer(
            m_device.Get(),
            instanceDescsSize,
            D3D12_RESOURCE_FLAG_NONE,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nv_helpers_dx12::kUploadHeapProps
        );

    // After all the buffers are allocated, or if only an update is required, we
    // can build the acceleration structure. Note that in the case of the update
    // we also pass the existing AS as the 'previous' AS, so that it can be
    // refitted in place.
    m_topLevelASGenerator.Generate(m_commandList.Get(), m_topLevelASBuffers.pScratch.Get(), m_topLevelASBuffers.pResult.Get(), m_topLevelASBuffers.pInstanceDesc.Get());
}

//-----------------------------------------------------------------------------
//
// Combine the BLAS and TLAS builds to construct the entire acceleration
// structure required to raytrace the scene
//
void CreateAccelerationStructures()
{
    // Build the bottom AS from the Triangle vertex buffer
    AccelerationStructureBuffers bottomLevelBuffers = CreateBottomLevelAS({
            {m_vertexBuffer.Get(), 3}
        });

    // Build the bottom AS from the Plane vertex buffer
    AccelerationStructureBuffers bottomLevelPlaneBuffers = CreateBottomLevelAS({
            {m_planeBuffer.Get(), 6}
        });

    // Just one instance for now
    m_instances = {
        {
            bottomLevelBuffers.pResult,
            XMMatrixIdentity()
        }
        {
            bottomLevelBuffers.pResult,
            XMMatrixTranslation(-.6f, 0, 0)
        }
        {
            bottomLevelBuffers.pResult,
            XMMatrixTranslation(.6f, 0, 0)
        }
        {
            bottomLevelPlaneBuffers.pResult,
            XMMatrixTranslation(0, 0, 0)
        }
    };
    CreateTopLevelAS(m_instances);

    // Flush the command list and wait for it to finish
    m_commandList->Close();
    ID3D12CommandList *ppCommandLists[] = {m_commandList.Get()};
    m_commandQueue->ExecuteCommandLists(1, ppCommandLists);

    m_fenceValue++;
    m_commandQueue->Signal(m_fence.Get(), m_fenceValue);
    m_fence->SetEventOnCompletion(m_fenceValue, m_fenceEvent);
    WaitForSingleObject(m_fenceEvent, INFINITE);

    // Once the command list is finished executing, reset it to be reused for
    // rendering
    ThrowIfFailed( m_commandList->Reset(m_commandAllocator.Get(), m_pipelineState.Get()));

    // Store the AS buffers. The rest of the buffers will be released once we exit
    // the function
    m_bottomLevelAS = bottomLevelBuffers.pResult;
}

void OnInit()
{
    LoadPipeline();
    LoadAssets();

    // Check the raytracing capabilities of the device
    CheckRaytracingSupport();

    // Setup the acceleration structures (AS) for raytracing. When setting up
    // geometry, each bottom-level AS has its own transform matrix.
    CreateAccelerationStructures();

    // Command lists are created in the recording state, but there is
    // nothing to record yet. The main loop expects it to be closed, so
    // close it now.
    ThrowIfFailed(m_commandList->Close());

    // Create the raytracing pipeline, associating the shader code to symbol names
    // and to their root signatures, and defining the amount of memory carried by
    // rays (ray payload)
    CreateRaytracingPipeline();

    // Allocate the buffer storing the raytracing output, with the same dimensions
    // as the target image
    CreateRaytracingOutputBuffer();
    // Create the buffer containing the raytracing result (always output in a
    // UAV), and create the heap referencing the resources used by the raytracing,
    // such as the acceleration structure
    CreateShaderResourceHeap();

    // Create the shader binding table and indicating which shaders
    // are invoked for each instance in the AS
    CreateShaderBindingTable();
}

ComPtr<idxcblob> m_rayGenLib;
ComPtr<idxcblob> m_hitLib;
ComPtr<idxcblob> m_missLib;

ComPtr<id3d12rootsignature> m_rayGenSignature;
ComPtr<id3d12rootsignature> m_hitSignature;
ComPtr<id3d12rootsignature> m_missSignature;

ComPtr<id3d12stateobject> m_rtStateObject;
ComPtr<id3d12stateobjectproperties> m_rtStateObjectProps;

//-----------------------------------------------------------------------------
// The ray generation shader needs to access 2 resources: the raytracing output
// and the top-level acceleration structure
ComPtr<id3d12rootsignature> CreateRayGenSignature()
{
    nv_helpers_dx12::RootSignatureGenerator rsc;
    rsc.AddHeapRangesParameter({
            {0 /*u0*/, 1 /*1 descriptor */, 0 /*use the implicit register space 0*/, D3D12_DESCRIPTOR_RANGE_TYPE_UAV /* UAV representing the output buffer*/, 0 /*heap slot where the UAV is defined*/},
            {0 /*t0*/, 1, 0, D3D12_DESCRIPTOR_RANGE_TYPE_SRV /*Top-level acceleration structure*/, 1}
        });

    return rsc.Generate(m_device.Get(), true);
}

//-----------------------------------------------------------------------------
// The hit shader communicates only through the ray payload, and therefore does
// not require any resources
ComPtr<id3d12rootsignature> CreateHitSignature()
{
    nv_helpers_dx12::RootSignatureGenerator rsc;
    rsc.AddRootParameter(D3D12_ROOT_PARAMETER_TYPE_SRV);

    return rsc.Generate(m_device.Get(), true);
}

//-----------------------------------------------------------------------------
// The miss shader communicates only through the ray payload, and therefore
// does not require any resources
ComPtr<id3d12rootsignature> CreateMissSignature()
{
    nv_helpers_dx12::RootSignatureGenerator rsc;
    return rsc.Generate(m_device.Get(), true);
}

//-----------------------------------------------------------------------------
// The raytracing pipeline binds the shader code, root signatures and pipeline
// characteristics in a single structure used by DXR to invoke the shaders and
// manage temporary memory during raytracing
void CreateRaytracingPipeline()
{
    nv_helpers_dx12::RayTracingPipelineGenerator pipeline(m_device.Get());

    // The pipeline contains the DXIL code of all the shaders potentially executed
    // during the raytracing process. This section compiles the HLSL code into a
    // set of DXIL libraries. We chose to separate the code in several libraries by
    // semantic (ray generation, hit, miss) for clarity. Any code layout can be
    // used.
    m_rayGenLibrary = nv_helpers_dx12::CompileShaderLibrary(L"RayGen.hlsl");
    m_missLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Miss.hlsl");
    m_hitLibrary = nv_helpers_dx12::CompileShaderLibrary(L"Hit.hlsl");

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    //As described at the beginning of this section, to each shader corresponds a root signature defining
    //its external inputs.
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // To be used, each DX12 shader needs a root signature defining which
    // parameters and buffers will be accessed.
    m_rayGenSignature = CreateRayGenSignature();
    m_missSignature = CreateMissSignature();
    m_hitSignature = CreateHitSignature();

    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // To be used, each shader needs to be associated to its root signature. Each shaders imported from
    // the DXIL libraries needs to be associated with exactly one root signature.
    // The shaders comprising the hit groups need to share the same root signature, which is associated
    // to the hit group (and not to the shaders themselves). Note that a shader does not have to actually
    // access all the resources declared in its root signature, as long as the root signature defines a
    // superset of the resources the shader needs.
    //~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // The following section associates the root signature to each shader.
    // Note that we can explicitly show that some shaders share the same root signature
    // (eg. Miss and ShadowMiss). Note that the hit shaders are now only referred
    // to as hit groups, meaning that the underlying intersection, any-hit and
    // closest-hit shaders share the same root signature.
    pipeline.AddRootSignatureAssociation(m_rayGenSignature.Get(), {L"RayGen"});
    pipeline.AddRootSignatureAssociation(m_missSignature.Get(), {L"Miss"});
    pipeline.AddRootSignatureAssociation(m_hitSignature.Get(), {L"HitGroup"});
}

ComPtr<id3d12resource> m_outputResource;
ComPtr<id3d12descriptorheap> m_srvUavHeap;

//-----------------------------------------------------------------------------
// Allocate the buffer holding the raytracing output, with the same size as the
// output image
void CreateRaytracingOutputBuffer()
{
    D3D12_RESOURCE_DESC resDesc = {};
    resDesc.DepthOrArraySize = 1;
    resDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;

    // The backbuffer is actually DXGI_FORMAT_R8G8B8A8_UNORM_SRGB, but sRGB
    // formats cannot be used with UAVs. For accuracy we should convert to sRGB
    // ourselves in the shader
    resDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    resDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
    resDesc.Width = GetWidth();
    resDesc.Height = GetHeight();
    resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    resDesc.MipLevels = 1;
    resDesc.SampleDesc.Count = 1;
    ThrowIfFailed(m_device->CreateCommittedResource(
                &nv_helpers_dx12::kDefaultHeapProps,
                D3D12_HEAP_FLAG_NONE,
                &resDesc,
                D3D12_RESOURCE_STATE_COPY_SOURCE,
                nullptr,
                IID_PPV_ARGS(&m_outputResource)
            ));
}

//-----------------------------------------------------------------------------
// Create the main heap used by the shaders, which will give access to the
// raytracing output and the top-level acceleration structure
void CreateShaderResourceHeap()
{
    // Create a SRV/UAV/CBV descriptor heap. We need 2 entries - 1 UAV for the
    // raytracing output and 1 SRV for the TLAS
    m_srvUavHeap = nv_helpers_dx12::CreateDescriptorHeap( m_device.Get(), 2, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, true);

    // Get a handle to the heap memory on the CPU side, to be able to write the
    // descriptors directly
    D3D12_CPU_DESCRIPTOR_HANDLE srvHandle = m_srvUavHeap->GetCPUDescriptorHandleForHeapStart();

    // Create the UAV. Based on the root signature we created it is the first
    // entry. The Create*View methods write the view information directly into
    // srvHandle
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
    m_device->CreateUnorderedAccessView(m_outputResource.Get(), nullptr, &uavDesc, srvHandle);

    // Add the Top Level AS SRV right after the raytracing output buffer
    srvHandle.ptr += m_device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc;
    srvDesc.Format = DXGI_FORMAT_UNKNOWN;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_RAYTRACING_ACCELERATION_STRUCTURE;
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.RaytracingAccelerationStructure.Location = m_topLevelASBuffers.pResult->GetGPUVirtualAddress();

    // Write the acceleration structure view in the heap
    m_device->CreateShaderResourceView(nullptr, &srvDesc, srvHandle);
}

nv_helpers_dx12::ShaderBindingTableGenerator m_sbtHelper;
ComPtr<id3d12resource> m_sbtStorage;

//-----------------------------------------------------------------------------
// The Shader Binding Table (SBT) is the cornerstone of the raytracing setup:
// this is where the shader resources are bound to the shaders, in a way that
// can be interpreted by the raytracer on GPU. In terms of layout, the SBT
// contains a series of shader IDs with their resource pointers. The SBT
// contains the ray generation shader, the miss shaders, then the hit groups.
// Using the helper class, those can be specified in arbitrary order.
void CreateShaderBindingTable()
{
    // The SBT helper class collects calls to Add*Program. If called several
    // times, the helper must be emptied before re-adding shaders.
    m_sbtHelper.Reset();

    // The pointer to the beginning of the heap is the only parameter required by
    // shaders without root parameters
    D3D12_GPU_DESCRIPTOR_HANDLE srvUavHeapHandle = m_srvUavHeap->GetGPUDescriptorHandleForHeapStart();

    // ??
    pointer heapPointer = (srvUavHeapHandle.ptr);

    // >~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // We can now add the various programs used in our example: according to its root signature, the ray generation shader needs to access
    // the raytracing output buffer and the top-level acceleration structure referenced in the heap. Therefore, it
    // takes a single resource pointer towards the beginning of the heap data. The miss shader and the hit group
    // only communicate through the ray payload, and do not require any resource, hence an empty resource array.
    // Note that the helper will group the shaders by types in the SBT, so it is possible to declare them in an
    // arbitrary order. For example, miss programs can be added before or after ray generation programs without
    // affecting the result.
    // However, within a given type (say, the hit groups), the order in which they are added
    // is important. It needs to correspond to the `InstanceContributionToHitGroupIndex` value used when adding
    // instances to the top-level acceleration structure: for example, an instance having `InstanceContributionToHitGroupIndex==0`
    // needs to have its hit group added first in the SBT.
    // ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
    // The ray generation only uses heap data
    m_sbtHelper.AddRayGenerationProgram(L"RayGen", {heapPointer});

    // The miss and hit shaders do not access any external resources: instead they
    // communicate their results through the ray payload
    m_sbtHelper.AddMissProgram(L"Miss", {});

    // Adding the triangle hit shader
    m_sbtHelper.AddHitGroup(L"HitGroup", {(void*)(m_vertexBuffer->GetGPUVirtualAddress())});
}

void PopulateCommandList()
{
    // Bind the descriptor heap giving access to the top-level acceleration
    // structure, as well as the raytracing output
    std::vector<id3d12descriptorheap*> heaps = {m_srvUavHeap.Get()};
    m_commandList->SetDescriptorHeaps(static_cast<uint>(heaps.size()), heaps.data());

    // On the last frame, the raytracing output was used as a copy source, to
    // copy its contents into the render target. Now we need to transition it to
    // a UAV so that the shaders can write in it.
    CD3DX12_RESOURCE_BARRIER transition = CD3DX12_RESOURCE_BARRIER::Transition(
        m_outputResource.Get(),
        D3D12_RESOURCE_STATE_COPY_SOURCE,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS
    );
    m_commandList->ResourceBarrier(1, &transition);

    // Setup the raytracing task
    D3D12_DISPATCH_RAYS_DESC desc = {};
    // The layout of the SBT is as follows: ray generation shader, miss
    // shaders, hit groups. As described in the CreateShaderBindingTable method,
    // all SBT entries of a given type have the same size to allow a fixed stride.
    // The ray generation shaders are always at the beginning of the SBT.
    uint32_t rayGenerationSectionSizeInBytes = m_sbtHelper.GetRayGenSectionSize();
    desc.RayGenerationShaderRecord.StartAddress = m_sbtStorage->GetGPUVirtualAddress();
    desc.RayGenerationShaderRecord.SizeInBytes = rayGenerationSectionSizeInBytes;

    // The miss shaders are in the second SBT section, right after the ray
    // generation shader. We have one miss shader for the camera rays and one
    // for the shadow rays, so this section has a size of 2*m_sbtEntrySize. We
    // also indicate the stride between the two miss shaders, which is the size
    // of a SBT entry
    uint32_t missSectionSizeInBytes = m_sbtHelper.GetMissSectionSize();
    desc.MissShaderTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes;
    desc.MissShaderTable.SizeInBytes = missSectionSizeInBytes;
    desc.MissShaderTable.StrideInBytes = m_sbtHelper.GetMissEntrySize();

    // The hit groups section start after the miss shaders. In this sample we
    // have one 1 hit group for the triangle
    uint32_t hitGroupsSectionSize = m_sbtHelper.GetHitGroupSectionSize();
    desc.HitGroupTable.StartAddress = m_sbtStorage->GetGPUVirtualAddress() + rayGenerationSectionSizeInBytes + missSectionSizeInBytes;
    desc.HitGroupTable.SizeInBytes = hitGroupsSectionSize;
    desc.HitGroupTable.StrideInBytes = m_sbtHelper.GetHitGroupEntrySize();

    // Dimensions of the image to render, identical to a kernel launch dimension
    desc.Width = GetWidth();
    desc.Height = GetHeight();
    desc.Depth = 1;

    // Bind the raytracing pipeline
    m_commandList->SetPipelineState1(m_rtStateObject.Get());
    // Dispatch the rays and write to the raytracing output
    m_commandList->DispatchRays(&desc);

    // The raytracing output needs to be copied to the actual render target used
    // for display. For this, we need to transition the raytracing output from a
    // UAV to a copy source, and the render target buffer to a copy destination.
    // We can then do the actual copy, before transitioning the render target
    // buffer into a render target, that will be then used to display the image
    // (This should be batched instead of all called one after the other)
    transition = CD3DX12_RESOURCE_BARRIER::Transition(
        m_outputResource.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_COPY_SOURCE
    );
    m_commandList->ResourceBarrier(1, &transition);

    transition = CD3DX12_RESOURCE_BARRIER::Transition(
        m_renderTargets[m_frameIndex].Get(),
        D3D12_RESOURCE_STATE_RENDER_TARGET,
        D3D12_RESOURCE_STATE_COPY_DEST
    );
    m_commandList->ResourceBarrier(1, &transition);

    // Perform the actual copy from the RayTrace output to the RenderTarget
    m_commandList->CopyResource(m_renderTargets[m_frameIndex].Get(), m_outputResource.Get());

    transition = CD3DX12_RESOURCE_BARRIER::Transition( m_renderTargets[m_frameIndex].Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_RENDER_TARGET);
    m_commandList->ResourceBarrier(1, &transition);
}

// #DXR Extra: Indexed Geometry
void CreateMengerSpongeVB()
{
    std::vector<Vertex> vertices;
    std::vector<UINT> indices;
    nv_helpers_dx12::GenerateMengerSponge(3, 0.75, vertices, indices);
    {
        const UINT mengerVBSize = static_cast<uint>(vertices.size()) * sizeof(Vertex);
        // Note: using upload heaps to transfer static data like vert buffers is not
        // recommended. Every time the GPU needs it, the upload heap will be
        // marshalled over. Please read up on Default Heap usage. An upload heap is
        // used here for code simplicity and because there are very few verts to
        // actually transfer.
        CD3DX12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferResource = CD3DX12_RESOURCE_DESC::Buffer(mengerVBSize);
        ThrowIfFailed(m_device->CreateCommittedResource( &heapProperty, D3D12_HEAP_FLAG_NONE, &bufferResource, // D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_mengerVB)));

        // Copy the triangle data to the vertex buffer.
        UINT8* pVertexDataBegin;
        CD3DX12_RANGE readRange(0, 0);

        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_mengerVB->Map(0, &readRange, reinterpret_cast<void**>(&pVertexDataBegin)));
        memcpy(pVertexDataBegin, vertices.data(), mengerVBSize);
        m_mengerVB->Unmap(0, nullptr);

        // Initialize the vertex buffer view.
        m_mengerVBView.BufferLocation = m_mengerVB->GetGPUVirtualAddress();
        m_mengerVBView.StrideInBytes = sizeof(Vertex);
        m_mengerVBView.SizeInBytes = mengerVBSize;
    }
    {
        const UINT mengerIBSize = static_cast<uint>(indices.size()) * sizeof(UINT);

        // Note: using upload heaps to transfer static data like vert buffers is not
        // recommended. Every time the GPU needs it, the upload heap will be
        // marshalled over. Please read up on Default Heap usage. An upload heap is
        // used here for code simplicity and because there are very few verts to
        // actually transfer.
        CD3DX12_HEAP_PROPERTIES heapProperty = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
        CD3DX12_RESOURCE_DESC bufferResource = CD3DX12_RESOURCE_DESC::Buffer(mengerIBSize);
        ThrowIfFailed(m_device->CreateCommittedResource( &heapProperty, D3D12_HEAP_FLAG_NONE, &bufferResource, // D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&m_mengerIB)));

        // Copy the triangle data to the index buffer.
        UINT8* pIndexDataBegin;
        CD3DX12_RANGE readRange(0, 0);

        // We do not intend to read from this resource on the CPU.
        ThrowIfFailed(m_mengerIB->Map(0, &readRange, reinterpret_cast<void**>(&pIndexDataBegin)));
        memcpy(pIndexDataBegin, indices.data(), mengerIBSize);
        m_mengerIB->Unmap(0, nullptr);

        // Initialize the index buffer view.
        m_mengerIBView.BufferLocation = m_mengerIB->GetGPUVirtualAddress();
        m_mengerIBView.Format = DXGI_FORMAT_R32_UINT;
        m_mengerIBView.SizeInBytes = mengerIBSize;
        m_mengerIndexCount = static_cast<uint>(indices.size());
        m_mengerVertexCount = static_cast<uint>(vertices.size());
    }
}
