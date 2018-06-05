//
// Created by cpasjuste on 17/04/18.
//

#ifndef OPENBOR_SHADER_H
#define OPENBOR_SHADER_H

#include <vita2d.h>

#include "vita-shader-collection/includes/lcd3x_v.h"
#include "vita-shader-collection/includes/lcd3x_f.h"
#include "vita-shader-collection/includes/texture_v.h"
#include "vita-shader-collection/includes/texture_f.h"
#include "vita-shader-collection/includes/sharp_bilinear_f.h"
#include "vita-shader-collection/includes/sharp_bilinear_v.h"
#include "vita-shader-collection/includes/sharp_bilinear_simple_f.h"
#include "vita-shader-collection/includes/sharp_bilinear_simple_v.h"

typedef struct vita2d_shader_input {
    const SceGxmProgramParameter *video_size;
    const SceGxmProgramParameter *texture_size;
    const SceGxmProgramParameter *output_size;
} vita2d_shader_input;

typedef struct vita2d_shader {
    SceGxmShaderPatcherId vertexProgramId;
    SceGxmShaderPatcherId fragmentProgramId;
    SceGxmVertexProgram *vertexProgram;
    SceGxmFragmentProgram *fragmentProgram;
    const SceGxmProgramParameter *paramPositionAttribute;
    const SceGxmProgramParameter *paramTexcoordAttribute;
    const SceGxmProgramParameter *wvpParam;
    vita2d_shader_input vertexInput;
    vita2d_shader_input fragmentInput;
} vita2d_shader;

vita2d_shader *vita2d_create_shader(const SceGxmProgram *vertexProgramGxp, const SceGxmProgram *fragmentProgramGxp);

void vita2d_free_shader(vita2d_shader *shader);

void vita2d_set_vertexUniform(
        const SceGxmProgramParameter *program_parameter, const float *value, unsigned int length);

void vita2d_set_fragmentUniform(
        const SceGxmProgramParameter *program_parameter, const float *value, unsigned int length);

void vita2d_apply_shader(const vita2d_shader *shader, float tex_w, float tex_h, float scale_x, float scale_y);

void vita2d_draw_texture_with_shader(const vita2d_shader *shader, const vita2d_texture *texture,
                                     float x, float y, float x_scale, float y_scale);

#endif //OPENBOR_SHADER_H
