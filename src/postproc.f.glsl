// Copyright (c) 2015 Sergio Gonzalez. All rights reserved.
// License: https://github.com/serge-rgb/milton#license

uniform sampler2D u_canvas;
uniform vec2 u_screen_size;

// FXAA disabled - pass through texture directly
void
main()
{
    vec2 coord = gl_FragCoord.xy / u_screen_size;
    out_color = texture(u_canvas, coord, 0);
}
