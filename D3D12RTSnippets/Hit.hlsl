#include "Common.hlsl"

struct STriVertex
{
    float3 vertex;
    float4 color;
};
StructuredBuffer<STriVertex> BTriVertex : register(t0);

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
    uint A = BTriVertex[(3 * PrimitiveIndex()) + 0].color;
    uint B = BTriVertex[(3 * PrimitiveIndex()) + 1].color;
    uint C = BTriVertex[(3 * PrimitiveIndex()) + 2].color;

    float2 barycentrics = attrib.bary;
    switch(InstanceID())
    {
        case 0:
            float3 hitColor = A * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
            break;

        case 1:
            float3 hitColor = B * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
            break;

        case 2:
            float3 hitColor = C * barycentrics.x + B * barycentrics.y + C * barycentrics.z;
            break;
    }

    payload.colorAndDistance = float4(hitColor.x, hitColor.y, hitColor.z, RayTCurrent());
}
