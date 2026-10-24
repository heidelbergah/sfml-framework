// grade.frag  -  Mud Cats
//
// The last pass over the frame. A cold, drained grade over everything, a
// vignette, a little grain, and the things that happen to the player's
// eyes: the edges close in and colour drains away when rounds are passing
// close, red creeps in when hurt, a close explosion whites everything out
// and leaves it doubled for a moment.
//
// Weather goes here too. On the fields at dawn a mist hangs everywhere
// except close around the player, thinning as the morning goes; on the
// Ochre line the rain makes everything a little darker and colder.

uniform sampler2D texture;
uniform vec2  resolution;
uniform float time;
uniform float suppression;   // 0..1
uniform float hurt;          // 0..1
uniform float flash;         // 0..1
uniform float shock;         // 0..1
uniform float dying;         // 0..1, when down and bleeding
uniform float mist;          // 0..1
uniform vec2  eye;           // the player, in frame pixels (y up)
uniform float reach;         // how far you can see, in frame pixels
uniform float rain;          // 0 or 1

float hash(vec2 p)
{
    return fract(sin(dot(p, vec2(12.9898, 78.233))) * 43758.5453);
}

void main()
{
    vec2 uv = gl_TexCoord[0].xy;
    vec3 c  = texture2D(texture, uv).rgb;

    // A close blast leaves the picture doubled for a moment.
    if(shock > 0.01)
    {
        vec2 off = vec2(sin(time * 23.0), cos(time * 19.0)) * shock * 2.5 / resolution;
        c = mix(c, texture2D(texture, uv + off).rgb, 0.5 * shock);
    }

    // Mist: clear close by, milky toward the edge of what you can see.
    if(mist > 0.01)
    {
        vec2  p   = uv * resolution;
        float far = smoothstep(reach * 0.3, reach * 1.05, length(p - eye));
        float drift = 0.85 + 0.15 * sin(p.x * 0.03 + time * 0.2) * sin(p.y * 0.05 - time * 0.13);
        c = mix(c, vec3(0.62, 0.66, 0.68), clamp(mist * far * drift, 0.0, 0.7));
    }
    if(rain > 0.5) c = c * vec3(0.9, 0.93, 1.0);

    float lum = dot(c, vec3(0.299, 0.587, 0.114));

    // The base grade: a fifth of the colour gone, shadows pulled cold.
    float drain = 0.2 + 0.45 * suppression + 0.5 * dying;
    c = mix(c, vec3(lum), drain);
    c = mix(c, c * vec3(0.92, 0.96, 1.05), 0.6);

    // Vignette, tighter under fire.
    vec2  d = uv - 0.5;
    float r = dot(d, d);
    float edge = smoothstep(0.12 - 0.07 * suppression - 0.06 * dying, 0.55, r);
    c *= 1.0 - edge * (0.55 + 0.35 * suppression + 0.3 * dying);

    // Hurt: red bleeding in from the edges.
    c = mix(c, vec3(0.45, 0.02, 0.02), clamp(edge * hurt * 1.4 + dying * edge * 0.6, 0.0, 0.8));

    // Grain, stepped to whole pixels.
    vec2 px = floor(uv * resolution);
    float g = hash(px + floor(time * 24.0)) - 0.5;
    c += g * (0.035 + 0.04 * suppression);

    c = mix(c, vec3(1.0, 0.97, 0.9), flash * 0.85);
    gl_FragColor = vec4(c, 1.0);
}
