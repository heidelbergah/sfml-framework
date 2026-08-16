// bloom_extract.frag
//
// Pass 1 of 3: keep only the bright pixels, and blur them horizontally.
//
// Combining the bright-pass with the first blur direction saves a full
// screen pass. Weights are the standard 9-tap Gaussian.

uniform sampler2D texture;
uniform vec2      texel_size;   // 1.0 / render target size, in pixels
uniform float     threshold;    // luminance below this contributes nothing
uniform float     soft_knee;    // width of the rolloff around the threshold

const float w0 = 0.227027;
const float w1 = 0.1945946;
const float w2 = 0.1216216;
const float w3 = 0.054054;
const float w4 = 0.016216;

vec3 bright_pass(vec2 uv)
{
    vec4 color = texture2D(texture, uv);

    float brightness = dot(color.rgb, vec3(0.2126, 0.7152, 0.0722));

    // smoothstep instead of a hard cutoff, so a pixel drifting across
    // the threshold fades in rather than popping.
    float contribution = smoothstep(threshold, threshold + soft_knee, brightness);

    return color.rgb * contribution;
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    float dx = texel_size.x;

    vec3 result = bright_pass(uv) * w0;

    result += bright_pass(uv + vec2(dx * 1.0, 0.0)) * w1;
    result += bright_pass(uv - vec2(dx * 1.0, 0.0)) * w1;
    result += bright_pass(uv + vec2(dx * 2.0, 0.0)) * w2;
    result += bright_pass(uv - vec2(dx * 2.0, 0.0)) * w2;
    result += bright_pass(uv + vec2(dx * 3.0, 0.0)) * w3;
    result += bright_pass(uv - vec2(dx * 3.0, 0.0)) * w3;
    result += bright_pass(uv + vec2(dx * 4.0, 0.0)) * w4;
    result += bright_pass(uv - vec2(dx * 4.0, 0.0)) * w4;

    gl_FragColor = vec4(result, 1.0);
}
