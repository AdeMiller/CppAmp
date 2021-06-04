//--------------------------------------------------------------------------------------
// File: ParticleDrawCpu.hlsl
//
// Shaders for rendering the particle as point sprite
//
// Copyright (c) Microsoft Corporation. All rights reserved.
//--------------------------------------------------------------------------------------

struct VSParticleIn
{
    float4  color   : COLOR;
    uint    id      : SV_VERTEXID;
};

struct VSParticleDrawOut
{
    float3 pos			: POSITION;
    float4 color		: COLOR;
};

struct GSParticleDrawOut
{
    float2 tex			: TEXCOORD0;
    float4 color		: COLOR;
    float4 pos			: SV_POSITION;
};

struct PSParticleDrawIn
{
    float2 tex			: TEXCOORD0;
    float4 color		: COLOR;
};

Texture2D		            g_txDiffuse;

SamplerState g_samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Clamp;
    AddressV = Clamp;
};

cbuffer cb0
{
    row_major float4x4 g_mWorldViewProj;
    row_major float4x4 g_mInvView;   
	float4 g_Color;		 
};

cbuffer cb1
{
    static float g_fParticleRad = 10.0f;   
};

cbuffer cbImmutable
{
    static float3 g_positions[4] =
    {
        float3( -1, 1, 0 ),
        float3( 1, 1, 0 ),
        float3( -1, -1, 0 ),
        float3( 1, -1, 0 ),
    };
    
    static float2 g_texcoords[4] = 
    { 
        float2(0,0), 
        float2(1,0),
        float2(0,1),
        float2(1,1),
    };
};

ByteAddressBuffer g_bufPosVelo;

//
// Vertex shader for drawing the point-sprite particles
//
VSParticleDrawOut VSParticleDraw(VSParticleIn input)
{
	const int particleSize = 16 * 4; // Size of float_3 object as number of bytes.
    VSParticleDrawOut output; 

	output.pos.x = asfloat(g_bufPosVelo.Load(input.id*particleSize + 0));
	output.pos.y = asfloat(g_bufPosVelo.Load(input.id*particleSize + 4));
	output.pos.z = asfloat(g_bufPosVelo.Load(input.id*particleSize + 8));

	float vz = asfloat(g_bufPosVelo.Load(input.id*particleSize + 24)) / 9;
    output.color = lerp(float4(1,0.1,0.1,1), input.color, vz);
    
    return output;
}

//
// GS for rendering point sprite particles.  Takes a point and turns it into 2 tris.
//
[maxvertexcount(4)]
void GSParticleDraw(point VSParticleDrawOut input[1], inout TriangleStream<GSParticleDrawOut> SpriteStream)
{
    GSParticleDrawOut output;
    
    //
    // Emit two new triangles
    //
    for(int i=0; i<4; i++)
    {
        float3 position = g_positions[i] * g_fParticleRad;
        position = mul( position, (float3x3)g_mInvView ) + input[0].pos;
        output.pos = mul( float4(position,1.0), g_mWorldViewProj ); 

        output.color = g_Color;        
        output.tex = g_texcoords[i];
        SpriteStream.Append(output);
    }
    SpriteStream.RestartStrip();
}

//
// PS for drawing particles
//
float4 PSParticleDraw(PSParticleDrawIn input) : SV_Target
{   
    return g_txDiffuse.Sample( g_samLinear, input.tex ) * input.color;
}