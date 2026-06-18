#include "PBR.hlsli"

VS_OUT InterpolateVertex(VS_OUT a, VS_OUT b)
{
    VS_OUT o;
    o.vertex = 0.5f * (a.vertex + b.vertex);
    o.texcoord = 0.5f * (a.texcoord + b.texcoord);
    o.normal = normalize(0.5f * (a.normal + b.normal));
    o.position = 0.5f * (a.position + b.position);
    o.tangent = normalize(0.5f * (a.tangent + b.tangent));
    o.shadowTexcoord = 0.5f * (a.shadowTexcoord + b.shadowTexcoord);
    return o;
}

void ReprojectVertex(inout VS_OUT v)
{
    v.vertex = mul(float4(v.position, 1.0f), viewProjection);

    float4 lightClip = mul(float4(v.position, 1.0f), light_view_projection);
    lightClip /= lightClip.w;
    lightClip.y = -lightClip.y;
    lightClip.xy = 0.5f * lightClip.xy + 0.5f;
    v.shadowTexcoord = lightClip.xyz;
}

void ApplyDamageDent(inout VS_OUT v)
{
    [unroll]
    for (int holeIndex = 0; holeIndex < MaxDamageHoles; ++holeIndex)
    {
        if (holeIndex >= damageHoleCount)
        {
            break;
        }

        float4 hole = damageHoles[holeIndex];
        float radius = max(hole.w, 0.001f);
        float distanceToDent = distance(v.position, hole.xyz);
        float influence = saturate(1.0f - distanceToDent / radius);

        influence = influence * influence * (3.0f - 2.0f * influence);

        if (influence > 0.0f)
        {
            float3 surfaceNormal = normalize(v.normal);
            v.position -= surfaceNormal * damageHoleDepth * influence;
        }
    }

    ReprojectVertex(v);
}

void EmitTriangle(VS_OUT a, VS_OUT b, VS_OUT c, inout TriangleStream<VS_OUT> stream)
{
    ApplyDamageDent(a);
    ApplyDamageDent(b);
    ApplyDamageDent(c);

    stream.Append(a);
    stream.Append(b);
    stream.Append(c);
    stream.RestartStrip();
}

void EmitSubdividedTriangle(VS_OUT v0, VS_OUT v1, VS_OUT v2, inout TriangleStream<VS_OUT> stream)
{
    VS_OUT m01 = InterpolateVertex(v0, v1);
    VS_OUT m12 = InterpolateVertex(v1, v2);
    VS_OUT m20 = InterpolateVertex(v2, v0);

    EmitTriangle(v0, m01, m20, stream);
    EmitTriangle(m01, v1, m12, stream);
    EmitTriangle(m20, m12, v2, stream);
    EmitTriangle(m01, m12, m20, stream);
}

[maxvertexcount(48)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> stream)
{
    VS_OUT v0 = input[0];
    VS_OUT v1 = input[1];
    VS_OUT v2 = input[2];

    VS_OUT m01 = InterpolateVertex(v0, v1);
    VS_OUT m12 = InterpolateVertex(v1, v2);
    VS_OUT m20 = InterpolateVertex(v2, v0);

    EmitSubdividedTriangle(v0, m01, m20, stream);
    EmitSubdividedTriangle(m01, v1, m12, stream);
    EmitSubdividedTriangle(m20, m12, v2, stream);
    EmitSubdividedTriangle(m01, m12, m20, stream);
}
