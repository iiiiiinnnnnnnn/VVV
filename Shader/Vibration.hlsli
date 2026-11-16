#ifndef __VIBRATION_H__
#define __VIBRATION_H__

cbuffer CbVibration : register(b2)
{
    float3 vivDir;
    float vivPower;
};

#endif