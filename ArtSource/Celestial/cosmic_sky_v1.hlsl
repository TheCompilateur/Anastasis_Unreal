float3 viewDir = normalize(-CamVec);
float c = cos(SkyRotation), s = sin(SkyRotation);
float3 skyDir = float3(c*viewDir.x-s*viewDir.y, s*viewDir.x+c*viewDir.y, viewDir.z);
float2 uv = float2(atan2(skyDir.y,skyDir.x)/6.2831853+0.5,
                   asin(clamp(skyDir.z,-1.0,1.0))/3.14159265+0.5);
float seam = smoothstep(0.0,0.055,uv.x) * (1.0-smoothstep(0.945,1.0,uv.x));
float3 panorama = Texture2DSample(SkyTex, SkyTexSampler, uv).rgb;
float3 river = panorama * (0.34 * seam);
float grain = dot(panorama,float3(0.25,0.50,0.25));
river += pow(saturate(grain),2.0) * float3(0.012,0.009,0.025);

float2 farGrid = uv * float2(420.0,210.0);
float2 farCell = floor(farGrid), farLocal = frac(farGrid);
farCell.x = fmod(farCell.x,420.0);
float farKey = frac(sin(dot(farCell,float2(127.1,311.7)))*43758.5453);
float2 farPoint = float2(frac(sin(dot(farCell,float2(269.5,183.3)))*43758.5453),
                         frac(sin(dot(farCell,float2(419.2,371.9)))*43758.5453));
float farStar = step(0.979,farKey) * (1.0-smoothstep(0.018,0.17,length(farLocal-farPoint)));
float2 nearGrid = uv * float2(92.0,46.0);
float2 nearCell = floor(nearGrid), nearLocal = frac(nearGrid);
nearCell.x = fmod(nearCell.x,92.0);
float nearKey = frac(sin(dot(nearCell,float2(91.7,257.3)))*43758.5453);
float2 nearPoint = float2(frac(sin(dot(nearCell,float2(345.2,159.4)))*43758.5453),
                          frac(sin(dot(nearCell,float2(217.6,341.8)))*43758.5453));
float guide = step(0.979,nearKey) * (1.0-smoothstep(0.012,0.095,length(nearLocal-nearPoint)));
float poleFade = 1.0-smoothstep(0.94,1.0,abs(skyDir.z));
float3 stars = (farStar*float3(0.012,0.016,0.030)
              + guide*float3(0.065,0.070,0.105)) * poleFade;

float3 md = normalize(MoonDir.xyz);
float moonDot = clamp(dot(viewDir,md),-1.0,1.0);
float moonAngle = acos(moonDot);
float moonStage = exp(-pow(moonAngle/0.19,2.0));
float3 celestial = (river + stars) * (1.0-0.45*moonStage) * NightStrength;

float3 moon = 0.0;
if (MoonStrength > 0.001 && moonDot > 0.972)
{
    float3 axis = abs(md.z) < 0.98 ? float3(0,0,1) : float3(1,0,0);
    float3 right = normalize(cross(axis,md));
    float3 up = cross(md,right);
    float2 moonUv = 0.5 + float2(dot(viewDir,right),dot(viewDir,up)) / 0.17498;
    if (all(moonUv >= 0.0) && all(moonUv <= 1.0))
    {
        float4 tex = Texture2DSample(MoonTex,MoonTexSampler,moonUv);
        float edge = 1.0-smoothstep(0.475,0.50,length(moonUv-0.5));
        moon += tex.rgb * tex.a * edge * 0.105;
    }
    float rim = exp(-pow((moonAngle-0.0875)/0.011,2.0));
    float halo = exp(-pow(moonAngle/0.18,2.0));
    moon += rim*float3(0.010,0.009,0.021)
          + halo*float3(0.0030,0.0022,0.0070);
    moon *= MoonStrength;
}

float3 meteor = 0.0;
if (MeteorStrength > 0.001)
{
    float3 a = normalize(MeteorStart.xyz), b = normalize(MeteorEnd.xyz);
    float3 head = normalize(lerp(a,b,saturate(MeteorPhase)));
    float3 along = normalize(b-a);
    float x = dot(viewDir-head,along);
    float y = length((viewDir-head)-x*along);
    float trail = smoothstep(-0.20,-0.012,x)*(1.0-smoothstep(-0.003,0.004,x));
    float fine = exp(-pow(y/0.0025,2.0))*trail;
    float haze = exp(-pow(y/0.014,2.0))*trail;
    float fire = exp(-pow(length(viewDir-head)/0.008,2.0));
    meteor = (fine*float3(0.27,0.32,0.52)
            + haze*float3(0.012,0.022,0.050)
            + fire*float3(0.55,0.61,0.76))*MeteorStrength;
}

float az = atan2(skyDir.y,skyDir.x);
float wave = dot(skyDir,normalize(float3(-0.26,0.41,0.87)))
           - 0.13*sin(az*3.0+SkyRotation*0.34);
float filament = exp(-pow(wave/0.027,2.0));
float fringe = exp(-pow((wave-0.068)/0.065,2.0));
float pulse = 0.68+0.32*sin(az*17.0+skyDir.z*45.0+SkyRotation*0.56);
float3 veil = (filament*float3(0.010,0.032,0.055)
             + fringe*float3(0.014,0.006,0.029))*pulse*VeilStrength;

return celestial + moon + meteor + veil;