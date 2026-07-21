// PBRGS.hlsl

#include "PBR.hlsli"

VS_OUT InterpolateVertex(VS_OUT a, VS_OUT b)
{
    VS_OUT o;
    o.vertex = 0.5f * (a.vertex + b.vertex);
    o.texcoord = 0.5f * (a.texcoord + b.texcoord);
    o.normal = normalize(0.5f * (a.normal + b.normal));
    o.position = 0.5f * (a.position + b.position);
    o.tangent = normalize(0.5f * (a.tangent + b.tangent));
    return o;
}

void ReprojectVertex(inout VS_OUT v)
{
    v.vertex = mul(float4(v.position, 1.0f), viewProjection);

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
            float3 dentDirection = damageHoleDirections[holeIndex].xyz;
            if (dot(dentDirection, dentDirection) < 0.0001f)
            {
                dentDirection = -normalize(v.normal);
            }
            else
            {
                dentDirection = normalize(dentDirection);
            }

            v.position += dentDirection * damageHoleDepth * influence;
        }
    }

    ReprojectVertex(v);
}

void ApplyFlatShading(inout VS_OUT a, inout VS_OUT b, inout VS_OUT c)
{
    float3 faceNormal = cross(b.position - a.position, c.position - a.position);
    float3 averageNormal = a.normal + b.normal + c.normal;

    if (dot(faceNormal, faceNormal) < 0.0001f)
    {
        faceNormal = averageNormal;
    }

    faceNormal = normalize(faceNormal);
    averageNormal = normalize(averageNormal);

    if (dot(faceNormal, averageNormal) < 0.0f)
    {
        faceNormal = -faceNormal;
    }

    float3 tangent = a.tangent + b.tangent + c.tangent;

    if (dot(tangent, tangent) < 0.0001f)
    {
        tangent = a.tangent;
    }

    tangent = normalize(tangent);
    tangent = tangent - faceNormal * dot(tangent, faceNormal);

    if (dot(tangent, tangent) < 0.0001f)
    {
        tangent = a.tangent;
    }

    tangent = normalize(tangent);

    a.normal = faceNormal;
    b.normal = faceNormal;
    c.normal = faceNormal;

    a.tangent = tangent;
    b.tangent = tangent;
    c.tangent = tangent;
}

void EmitTriangle(VS_OUT a, VS_OUT b, VS_OUT c, inout TriangleStream<VS_OUT> stream)
{
    ApplyDamageDent(a);
    ApplyDamageDent(b);
    ApplyDamageDent(c);

    if (isFlatShading != 0)
    {
        ApplyFlatShading(a, b, c);
    }

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

    if (damageHoleCount <= 0)
    {
        EmitTriangle(v0, v1, v2, stream);
        return;
    }

    VS_OUT m01 = InterpolateVertex(v0, v1);
    VS_OUT m12 = InterpolateVertex(v1, v2);
    VS_OUT m20 = InterpolateVertex(v2, v0);

    EmitSubdividedTriangle(v0, m01, m20, stream);
    EmitSubdividedTriangle(m01, v1, m12, stream);
    EmitSubdividedTriangle(m20, m12, v2, stream);
    EmitSubdividedTriangle(m01, m12, m20, stream);
}
