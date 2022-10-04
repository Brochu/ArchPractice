#include "Common.hlsl"

[shader("closesthit")]
void ClosestHit(inout HitInfo payload, Attributes attrib)
{
  float2 b = attrib.bary;
  float r = b.x;
  float g = b.y;
  float b = 1.0 - r - g;

  payload.colorAndDistance = float4(r, g, b, RayTCurrent());
}
