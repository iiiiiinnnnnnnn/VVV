#include "PBR.hlsli"

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

[maxvertexcount(3)]
void main(triangle VS_OUT input[3], inout TriangleStream<VS_OUT> stream)
{
    VS_OUT a = input[0];
    VS_OUT b = input[1];
    VS_OUT c = input[2];
    if (isFlatShading != 0) ApplyFlatShading(a, b, c);
    stream.Append(a);
    stream.Append(b);
    stream.Append(c);
    stream.RestartStrip();
}
