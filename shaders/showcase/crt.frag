// crt.frag  -  Showcase
//
// An old television over a tiny scene drawn at quarter resolution:
// barrel distortion, alternate dark scanlines, a faint aperture-grille
// tint, a vignette, and static while the channel changes.
//
// This one deliberately does not quantise to the palette. Scanlines and
// static are the point, and they need the in-between shades.

uniform vec2      resolution;
uniform sampler2D screen;
uniform float     time;
uniform float     static_amount;
uniform vec4      bezel;

// Hash without sine (Dave Hoskins). The usual fract(sin(x) * 43758.5)
// loses precision on many GPUs once x grows, and the "noise" turns into
// visible stripes.
float hash(vec2 p)
{
    vec3 p3 = fract(vec3(p.xyx) * 0.1031);
    p3 += dot(p3, p3.yzx + 33.33);
    return fract((p3.x + p3.y) * p3.z);
}

void main()
{
    vec2 uv    = gl_TexCoord[0].xy;
    vec2 pixel = floor(uv * resolution);

    // Bulge outward from the centre.
    vec2 c = uv * 2.0 - 1.0;
    c *= 1.0 + dot(c, c) * 0.07;
    vec2 tube = c * 0.5 + 0.5;

    if(tube.x < 0.0 || tube.x > 1.0 || tube.y < 0.0 || tube.y > 1.0)
    {
        gl_FragColor = bezel;
        return;
    }

    vec3 color = texture2D(screen, vec2(tube.x, 1.0 - tube.y)).rgb;

    // Every other row dark.
    if(mod(pixel.y, 2.0) > 0.5) color *= 0.55;

    // Columns cycle through slight red, green and blue emphasis.
    float column = mod(pixel.x, 3.0);
    if(column < 0.5)      color *= vec3(1.06, 0.94, 0.94);
    else if(column < 1.5) color *= vec3(0.94, 1.06, 0.94);
    else                  color *= vec3(0.94, 0.94, 1.06);

    color *= 1.0 - dot(c, c) * 0.22;
    color *= 0.97 + 0.03 * sin(time * 50.0);

    float noise = hash(pixel + vec2(time * 97.0, time * 13.0));
    color = mix(color, vec3(noise * 0.85), static_amount);

    gl_FragColor = vec4(color, 1.0);
}
