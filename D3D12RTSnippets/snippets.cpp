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

    // Just one instance for now
    m_instances = {
        {
            bottomLevelBuffers.pResult,
            XMMatrixIdentity()
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
