#include "Common.hlsl"

[shader("miss")]
void Miss(inout HitInfo payload : SV_RayPayload)
{
    //Miss Shader
    //To slightly differentiate the raster and the raytracing, we will add a simple ramp color background by
    //modifying the `Miss` function: we simply obtain the
    //coordinates of the currently rendered pixel using and use them to compute a linear gradient:

    uint2 launchIndex = DispatchRaysIndex().xy;
    float2 dims = float2(DispatchRaysDimensions().xy);
    float ramp = launchIndex.y / dims.y;

    payload.colorAndDistance = float4(0.0f, 0.2f, 0.7f - 0.3f*ramp, -1.0f);
}
