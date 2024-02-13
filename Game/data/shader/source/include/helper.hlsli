static const float PI = 3.14159265358979;
static const float EPSILON = 0.000000000001;
static const uint vertexMax = 16777216;
static const uint clusterMax = 65536;
static const uint clusterSize = 64;
static const uint MAX_OBJ_NUM = 256;
static const float3 DEBUG_COL = float3(1,0,0);
static const uint SCREENWIDTH = 1600;
static const uint SCREENHEIGHT = 900;

float2 SphericalToEquirectangular(float3 v)
{
	float2 uv = float2(0.5 - atan2(v.x, v.z) / (2 * PI), acos(v.y) / PI);
	
	return uv;
}

float3 EquirectangularToSpherical(float2 uv)
{
	float3 v;
	v.x = cos(2 * PI * (0.5 - uv.x)) * sin(PI * uv.y);
	v.y = sin(2 * PI * (0.5 - uv.x)) * sin(PI * uv.y);
	v.z = cos(PI * uv.y);
	
	return v;
}

static const float3 DISTINCT_COLOR[64] = {
	{0.180392f, 0.133333f, 0.184314f},
	{0.243137f, 0.207843f, 0.27451f},
	{0.384314f, 0.333333f, 0.396078f},
	{0.588235f, 0.423529f, 0.423529f},
	{0.670588f, 0.580392f, 0.478431f},
	{0.411765f, 0.309804f, 0.384314f},
	{0.498039f, 0.439216f, 0.541176f},
	{0.607843f, 0.670588f, 0.698039f},
	{0.780392f, 0.862745f, 0.815686f},
	{1.f, 1.f, 1.f},
	{0.431373f, 0.152941f, 0.152941f},
	{0.701961f, 0.219608f, 0.192157f},
	{0.917647f, 0.309804f, 0.211765f},
	{0.960784f, 0.490196f, 0.290196f},
	{0.682353f, 0.137255f, 0.203922f},
	{0.909804f, 0.231373f, 0.231373f},
	{0.984314f, 0.419608f, 0.113725f},
	{0.968627f, 0.588235f, 0.0901961f},
	{0.976471f, 0.760784f, 0.168627f},
	{0.478431f, 0.188235f, 0.270588f},
	{0.619608f, 0.270588f, 0.223529f},
	{0.803922f, 0.407843f, 0.239216f},
	{0.901961f, 0.564706f, 0.305882f},
	{0.984314f, 0.72549f, 0.329412f},
	{0.298039f, 0.243137f, 0.141176f},
	{0.403922f, 0.4f, 0.2f},
	{0.635294f, 0.662745f, 0.278431f},
	{0.835294f, 0.878431f, 0.294118f},
	{0.984314f, 1.f, 0.52549f},
	{0.0862745f, 0.352941f, 0.298039f},
	{0.137255f, 0.564706f, 0.388235f},
	{0.117647f, 0.737255f, 0.45098f},
	{0.568627f, 0.858824f, 0.411765f},
	{0.803922f, 0.87451f, 0.423529f},
	{0.192157f, 0.211765f, 0.219608f},
	{0.215686f, 0.305882f, 0.290196f},
	{0.329412f, 0.494118f, 0.392157f},
	{0.572549f, 0.662745f, 0.517647f},
	{0.698039f, 0.729412f, 0.564706f},
	{0.0431373f, 0.368627f, 0.396078f},
	{0.0431373f, 0.541176f, 0.560784f},
	{0.054902f, 0.686275f, 0.607843f},
	{0.188235f, 0.882353f, 0.72549f},
	{0.560784f, 0.972549f, 0.886275f},
	{0.196078f, 0.2f, 0.32549f},
	{0.282353f, 0.290196f, 0.466667f},
	{0.301961f, 0.396078f, 0.705882f},
	{0.301961f, 0.607843f, 0.901961f},
	{0.560784f, 0.827451f, 1.f},
	{0.270588f, 0.160784f, 0.247059f},
	{0.419608f, 0.243137f, 0.458824f},
	{0.564706f, 0.368627f, 0.662745f},
	{0.658824f, 0.517647f, 0.952941f},
	{0.917647f, 0.678431f, 0.929412f},
	{0.458824f, 0.235294f, 0.329412f},
	{0.635294f, 0.294118f, 0.435294f},
	{0.811765f, 0.396078f, 0.498039f},
	{0.929412f, 0.501961f, 0.6f},
	{0.513726f, 0.109804f, 0.364706f},
	{0.764706f, 0.141176f, 0.329412f},
	{0.941176f, 0.309804f, 0.470588f},
	{0.964706f, 0.505882f, 0.505882f},
	{0.988235f, 0.654902f, 0.564706f},
	{0.992157f, 0.796078f, 0.690196f},
};

//channel will be range [-1,1]
float Pack3PNForFP32(float3 channel)
{
	//normalize channel to [0,1]
	channel = (channel + float3(1,1,1)) / 2.0f;

	// layout of a 32-bit fp register
	// SEEEEEEEEMMMMMMMMMMMMMMMMMMMMMMM
	// 1 sign bit; 8 bits for the exponent and 23 bits for the mantissa
	uint uValue;

	// pack x
	uValue = ((uint)(channel.x * 65535.0 + 0.5)); // goes from bit 0 to 15
	
	// pack y in EMMMMMMM
	uValue |= ((uint)(channel.y * 255.0 + 0.5)) << 16;

	// pack z in SEEEEEEE
	// the last E will never be 1b because the upper value is 254
	// max value is 11111110 == 254
	// this prevents the bits of the exponents to become all 1
	// range is 1.. 254
	// to prevent an exponent that is 0 we add 1.0
	uValue |= ((uint)(channel.z * 253.0 + 1.5)) << 24;

	return asfloat(uValue);
}

// unpack three positive normalized values from a 32-bit float
float3 Unpack3PNFromFP32(float fFloatFromFP32)
{
	float a, b, c, d;
	uint uValue;
	
	uint uInputFloat = asuint(fFloatFromFP32);
	
	// unpack a
	// mask out all the stuff above 16-bit with 0xFFFF
	a = ((uInputFloat) & 0xFFFF) / 65535.0;
	
	b = ((uInputFloat >> 16) & 0xFF) / 255.0;
	
	// extract the 1..254 value range and subtract 1
	// ending up with 0..253
	c = (((uInputFloat >> 24) & 0xFF) - 1.0) / 253.0;

	float3 result = float3(a, b, c);
	result = result * 2 - float3(1,1,1);

	return result;
}