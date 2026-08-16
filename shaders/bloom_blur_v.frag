// bloom_blur_v.frag
//
// Pass 2 of 3: vertical half of the separable Gaussian.
//
// A 9x9 kernel applied directly costs 81 texture fetches per pixel. The
// same kernel run as a horizontal pass then a vertical one costs 9 + 9,
// which is where the ~4.5x saving comes from.

uniform sampler2D texture;
uniform vec2      texel_size;
uniform float     intensity;

const float w0 = 0.227027;
const float w1 = 0.1945946;
const float w2 = 0.1216216;
const float w3 = 0.054054;
const float w4 = 0.016216;

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    float dy = texel_size.y;

    vec3 result = texture2D(texture, uv).rgb * w0;

    result += texture2D(texture, uv + vec2(0.0, dy * 1.0)).rgb * w1;
    result += texture2D(texture, uv - vec2(0.0, dy * 1.0)).rgb * w1;
    result += texture2D(texture, uv + vec2(0.0, dy * 2.0)).rgb * w2;
    result += texture2D(texture, uv - vec2(0.0, dy * 2.0)).rgb * w2;
    result += texture2D(texture, uv + vec2(0.0, dy * 3.0)).rgb * w3;
    result += texture2D(texture, uv - vec2(0.0, dy * 3.0)).rgb * w3;
    result += texture2D(texture, uv + vec2(0.0, dy * 4.0)).rgb * w4;
    result += texture2D(texture, uv - vec2(0.0, dy * 4.0)).rgb * w4;

    gl_FragColor = vec4(result * intensity, 1.0);
}
