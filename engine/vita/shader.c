//
// Created by cpasjuste on 17/04/18.
//

#include <malloc.h>
#include "shader.h"

extern float _vita2d_ortho_matrix[4 * 4];
extern const SceGxmProgramParameter *_vita2d_colorWvpParam;
extern void *indices_buf_addr;

// from frangarcj
vita2d_shader *vita2d_create_shader(const SceGxmProgram *vertexProgramGxp, const SceGxmProgram *fragmentProgramGxp) {

    vita2d_shader *shader = (vita2d_shader *) malloc(sizeof(*shader));
    SceGxmShaderPatcher *shaderPatcher = vita2d_get_shader_patcher();

    sceGxmProgramCheck(vertexProgramGxp);
    sceGxmProgramCheck(fragmentProgramGxp);
    sceGxmShaderPatcherRegisterProgram(shaderPatcher, vertexProgramGxp, &shader->vertexProgramId);
    sceGxmShaderPatcherRegisterProgram(shaderPatcher, fragmentProgramGxp, &shader->fragmentProgramId);

    shader->paramPositionAttribute = sceGxmProgramFindParameterByName(vertexProgramGxp, "aPosition");
    shader->paramTexcoordAttribute = sceGxmProgramFindParameterByName(vertexProgramGxp, "aTexcoord");

    shader->vertexInput.texture_size = sceGxmProgramFindParameterByName(vertexProgramGxp, "IN.texture_size");
    shader->vertexInput.output_size = sceGxmProgramFindParameterByName(vertexProgramGxp, "IN.output_size");
    shader->vertexInput.video_size = sceGxmProgramFindParameterByName(vertexProgramGxp, "IN.video_size");

    shader->fragmentInput.texture_size = sceGxmProgramFindParameterByName(fragmentProgramGxp, "IN.texture_size");
    shader->fragmentInput.output_size = sceGxmProgramFindParameterByName(fragmentProgramGxp, "IN.output_size");
    shader->fragmentInput.video_size = sceGxmProgramFindParameterByName(fragmentProgramGxp, "IN.video_size");

    // create texture vertex format
    SceGxmVertexAttribute textureVertexAttributes[2];
    SceGxmVertexStream textureVertexStreams[1];
    /* x,y,z: 3 float 32 bits */
    textureVertexAttributes[0].streamIndex = 0;
    textureVertexAttributes[0].offset = 0;
    textureVertexAttributes[0].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
    textureVertexAttributes[0].componentCount = 3; // (x, y, z)
    textureVertexAttributes[0].regIndex = (unsigned short)
            sceGxmProgramParameterGetResourceIndex(shader->paramPositionAttribute);
    /* u,v: 2 floats 32 bits */
    textureVertexAttributes[1].streamIndex = 0;
    textureVertexAttributes[1].offset = 12; // (x, y, z) * 4 = 11 bytes
    textureVertexAttributes[1].format = SCE_GXM_ATTRIBUTE_FORMAT_F32;
    textureVertexAttributes[1].componentCount = 2; // (u, v)
    textureVertexAttributes[1].regIndex = (unsigned short)
            sceGxmProgramParameterGetResourceIndex(shader->paramTexcoordAttribute);
    // 16 bit (short) indices
    textureVertexStreams[0].stride = sizeof(vita2d_texture_vertex);
    textureVertexStreams[0].indexSource = SCE_GXM_INDEX_SOURCE_INDEX_16BIT;

    // create texture shaders
    sceGxmShaderPatcherCreateVertexProgram(
            shaderPatcher,
            shader->vertexProgramId,
            textureVertexAttributes,
            2,
            textureVertexStreams,
            1,
            &shader->vertexProgram);

    // Fill SceGxmBlendInfo
    static const SceGxmBlendInfo blend_info = {
            .colorFunc = SCE_GXM_BLEND_FUNC_ADD,
            .alphaFunc = SCE_GXM_BLEND_FUNC_ADD,
            .colorSrc  = SCE_GXM_BLEND_FACTOR_SRC_ALPHA,
            .colorDst  = SCE_GXM_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA,
            .alphaSrc  = SCE_GXM_BLEND_FACTOR_ONE,
            .alphaDst  = SCE_GXM_BLEND_FACTOR_ZERO,
            .colorMask = SCE_GXM_COLOR_MASK_ALL
    };

    sceGxmShaderPatcherCreateFragmentProgram(
            shaderPatcher,
            shader->fragmentProgramId,
            SCE_GXM_OUTPUT_REGISTER_FORMAT_UCHAR4,
            SCE_GXM_MULTISAMPLE_NONE,
            &blend_info,
            vertexProgramGxp,
            &shader->fragmentProgram);

    shader->wvpParam = sceGxmProgramFindParameterByName(vertexProgramGxp, "wvp");

    return shader;
}

void vita2d_free_shader(vita2d_shader *shader) {

    SceGxmShaderPatcher *shaderPatcher = vita2d_get_shader_patcher();
    sceGxmShaderPatcherReleaseFragmentProgram(shaderPatcher, shader->fragmentProgram);
    sceGxmShaderPatcherReleaseVertexProgram(shaderPatcher, shader->vertexProgram);
    sceGxmShaderPatcherUnregisterProgram(shaderPatcher, shader->fragmentProgramId);
    sceGxmShaderPatcherUnregisterProgram(shaderPatcher, shader->vertexProgramId);
    free(shader);
}

void vita2d_set_vertexUniform(
        const SceGxmProgramParameter *program_parameter, const float *value, unsigned int length) {
    void *vertex_wvp_buffer;
    sceGxmReserveVertexDefaultUniformBuffer(vita2d_get_context(), &vertex_wvp_buffer);
    sceGxmSetUniformDataF(vertex_wvp_buffer, program_parameter, 0, length, value);
}

void vita2d_set_fragmentUniform(
        const SceGxmProgramParameter *program_parameter, const float *value, unsigned int length) {
    void *fragment_wvp_buffer;
    sceGxmReserveFragmentDefaultUniformBuffer(vita2d_get_context(), &fragment_wvp_buffer);
    sceGxmSetUniformDataF(fragment_wvp_buffer, program_parameter, 0, length, value);
}

void vita2d_apply_shader(const vita2d_shader *shader, float tex_w, float tex_h, float scale_x, float scale_y) {

    sceGxmSetVertexProgram(vita2d_get_context(), shader->vertexProgram);
    sceGxmSetFragmentProgram(vita2d_get_context(), shader->fragmentProgram);

    // set shader params
    float *tex_size = (float *) vita2d_pool_memalign(2 * sizeof(float), sizeof(float));
    tex_size[0] = tex_w;
    tex_size[1] = tex_h;

    float *out_size = (float *) vita2d_pool_memalign(2 * sizeof(float), sizeof(float));
    out_size[0] = tex_w * scale_x;
    out_size[1] = tex_h * scale_y;

    if (shader->vertexInput.texture_size > 0) {
        vita2d_set_vertexUniform(shader->vertexInput.texture_size, tex_size, 2);
    }
    if (shader->vertexInput.output_size > 0) {
        vita2d_set_vertexUniform(shader->vertexInput.output_size, out_size, 2);
    }
    if (shader->vertexInput.video_size > 0) {
        vita2d_set_vertexUniform(shader->vertexInput.video_size, tex_size, 2);
    }
    if (shader->fragmentInput.texture_size > 0) {
        vita2d_set_fragmentUniform(shader->fragmentInput.texture_size, tex_size, 2);
    }
    if (shader->fragmentInput.output_size > 0) {
        vita2d_set_fragmentUniform(shader->fragmentInput.output_size, out_size, 2);
    }
    if (shader->fragmentInput.video_size > 0) {
        vita2d_set_fragmentUniform(shader->fragmentInput.video_size, tex_size, 2);
    }
}

void vita2d_draw_texture_with_shader(const vita2d_shader *shader, const vita2d_texture *texture,
                                     float x, float y, float x_scale, float y_scale) {

    vita2d_texture_vertex *vertices = (vita2d_texture_vertex *) vita2d_pool_memalign(
            4 * sizeof(vita2d_texture_vertex), sizeof(vita2d_texture_vertex));

    const float tex_w = vita2d_texture_get_width(texture);
    const float tex_h = vita2d_texture_get_height(texture);
    const float w = x_scale * tex_w;
    const float h = y_scale * tex_h;

    vertices[0].x = x;
    vertices[0].y = y;
    vertices[0].z = +0.5f;
    vertices[0].u = 0.0f;
    vertices[0].v = 0.0f;

    vertices[1].x = x + w;
    vertices[1].y = y;
    vertices[1].z = +0.5f;
    vertices[1].u = 1.0f;
    vertices[1].v = 0.0f;

    vertices[2].x = x;
    vertices[2].y = y + h;
    vertices[2].z = +0.5f;
    vertices[2].u = 0.0f;
    vertices[2].v = 1.0f;

    vertices[3].x = x + w;
    vertices[3].y = y + h;
    vertices[3].z = +0.5f;
    vertices[3].u = 1.0f;
    vertices[3].v = 1.0f;

    vita2d_apply_shader(shader, tex_w, tex_h, x_scale, y_scale);

    void *vertexDefaultBuffer;
    sceGxmReserveVertexDefaultUniformBuffer(vita2d_get_context(), &vertexDefaultBuffer);
    sceGxmSetUniformDataF(vertexDefaultBuffer, _vita2d_colorWvpParam, 0, 16, _vita2d_ortho_matrix);

    // Set the texture to the TEXUNIT0
    sceGxmSetFragmentTexture(vita2d_get_context(), 0, &texture->gxm_tex);

    sceGxmSetVertexStream(vita2d_get_context(), 0, vertices);
    sceGxmDraw(vita2d_get_context(), SCE_GXM_PRIMITIVE_TRIANGLE_STRIP, SCE_GXM_INDEX_FORMAT_U16, indices_buf_addr, 4);
}
