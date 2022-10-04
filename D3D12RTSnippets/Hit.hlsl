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
  float2 barycentrics = attrib.bary;
  uint vertId = 3 * PrimitiveIndex();
  float3 hitColor =
      BTriVertex[vertId + 0].color * barycentrics.x +
      BTriVertex[vertId + 1].color * barycentrics.y +
      BTriVertex[vertId + 2].color * barycentrics.z;

  payload.colorAndDistance = float4(hitColor.x, hitColor.y, hitColor.z, RayTCurrent());
}
