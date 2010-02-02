/* cairo - a vector graphics library with display and print output
 *
 * Copyright © 2010 Eric Anholt
 * Copyright © 2009 T. Zachary Laine
 *
 * This library is free software; you can redistribute it and/or
 * modify it either under the terms of the GNU Lesser General Public
 * License version 2.1 as published by the Free Software Foundation
 * (the "LGPL") or, at your option, under the terms of the Mozilla
 * Public License Version 1.1 (the "MPL"). If you do not alter this
 * notice, a recipient may use your version of this file under either
 * the MPL or the LGPL.
 *
 * You should have received a copy of the LGPL along with this library
 * in the file COPYING-LGPL-2.1; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 * You should have received a copy of the MPL along with this library
 * in the file COPYING-MPL-1.1
 *
 * The contents of this file are subject to the Mozilla Public License
 * Version 1.1 (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://www.mozilla.org/MPL/
 *
 * This software is distributed on an "AS IS" basis, WITHOUT WARRANTY
 * OF ANY KIND, either express or implied. See the LGPL or the MPL for
 * the specific language governing rights and limitations.
 *
 * The Original Code is the cairo graphics library.
 *
 * The Initial Developer of the Original Code is T. Zachary Laine.
 */

#include "cairoint.h"
#include "cairo-gl-private.h"
#include "cairo-error-private.h"

static GLint
_cairo_gl_compile_glsl(GLenum type, GLint *shader_out, const char *source)
{
    GLint ok;
    GLint shader;

    shader = glCreateShaderObjectARB (type);
    glShaderSourceARB (shader, 1, (const GLchar **)&source, NULL);
    glCompileShaderARB (shader);
    glGetObjectParameterivARB (shader, GL_OBJECT_COMPILE_STATUS_ARB, &ok);
    if (!ok) {
	GLchar *info;
	GLint size;

	glGetObjectParameterivARB (shader, GL_OBJECT_INFO_LOG_LENGTH_ARB,
				   &size);
	info = malloc (size);

	if (info)
	    glGetInfoLogARB (shader, size, NULL, info);
	fprintf (stderr, "Failed to compile %s: %s\n",
		 type == GL_FRAGMENT_SHADER ? "FS" : "VS",
		 info);
	fprintf (stderr, "Shader source:\n%s", source);
	fprintf (stderr, "GLSL compile failure\n");

	glDeleteObjectARB (shader);

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *shader_out = shader;

    return CAIRO_STATUS_SUCCESS;
}

cairo_status_t
_cairo_gl_load_glsl (GLint *shader_out,
		     const char *vs_source, const char *fs_source)
{
    GLint ok;
    GLint shader, vs, fs;
    cairo_status_t status;

    shader = glCreateProgramObjectARB ();

    status = _cairo_gl_compile_glsl (GL_VERTEX_SHADER_ARB, &vs, vs_source);
    if (_cairo_status_is_error (status))
	goto fail;
    status = _cairo_gl_compile_glsl (GL_FRAGMENT_SHADER_ARB, &fs, fs_source);
    if (_cairo_status_is_error (status))
	goto fail;

    glAttachObjectARB (shader, vs);
    glAttachObjectARB (shader, fs);
    glLinkProgram (shader);
    glGetObjectParameterivARB (shader, GL_OBJECT_LINK_STATUS_ARB, &ok);
    if (!ok) {
	GLchar *info;
	GLint size;

	glGetObjectParameterivARB (shader, GL_OBJECT_INFO_LOG_LENGTH_ARB,
				   &size);
	info = malloc (size);

	if (info)
	    glGetInfoLogARB (shader, size, NULL, info);
	fprintf (stderr, "Failed to link: %s\n", info);
	free (info);
	status = CAIRO_INT_STATUS_UNSUPPORTED;

	return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    *shader_out = shader;

    return CAIRO_STATUS_SUCCESS;

fail:
    glDeleteObjectARB (shader);
    return status;
}

typedef struct _shader_impl {
    cairo_status_t
    (*compile_shader) (GLuint *shader, GLenum type, const char *text);

    cairo_status_t
    (*link_shader) (GLuint *program, GLuint vert, GLuint frag);

    void
    (*destroy_shader_program) (cairo_gl_shader_program_t *program);

    cairo_status_t
    (*create_linear_gradient_shader_program) (cairo_gl_shader_program_t *program);

    cairo_status_t
    (*create_radial_gradient_shader_program) (cairo_gl_shader_program_t *program);

    cairo_status_t
    (*bind_float_to_shader) (GLuint program, const char *name,
                             float value);

    cairo_status_t
    (*bind_vec2_to_shader) (GLuint program, const char *name,
                            float value0, float value1);

    cairo_status_t
    (*bind_vec3_to_shader) (GLuint program, const char *name,
                            float value0, float value1,
                            float value2);

    cairo_status_t
    (*bind_vec4_to_shader) (GLuint program, const char *name,
                            float value0, float value1,
                            float value2, float value3);

    cairo_status_t
    (*bind_matrix_to_shader) (GLuint program, const char *name, cairo_matrix_t* m);

    cairo_status_t
    (*bind_texture_to_shader) (GLuint program, const char *name, GLuint tex_unit);

    GLenum
    (*vertex_enumerator) (void);

    GLenum
    (*fragment_enumerator) (void);
} shader_impl_t;

static const shader_impl_t*
get_impl (void);

static const char * const minimal_vert_text_110 =
    "#version 110\n"
    "\n"
    "void main ()\n"
    "{ gl_Position = ftransform(); }\n";

/* This fragment shader was adapted from Argiris Kirtzidis' cairo-gral
 * library, found at git://github.com/akyrtzi/cairo-gral.git.  Argiris' shader
 * was adapted from the original algorithm in pixman.
 */
static const char * const radial_gradient_frag_text_110 =
    "#version 110\n"
    "\n"
    "uniform sampler1D tex;\n"
    "uniform mat4 matrix;\n"
    "uniform vec2 circle_1;\n"
    "uniform float radius_0;\n"
    "uniform float radius_1;\n"
    "uniform float first_offset;\n"
    "uniform float last_offset;\n"
    "\n"
    "void main ()\n"
    "{\n"
    "    vec2 pos = (matrix * vec4 (gl_FragCoord.xy, 0.0, 1.0)).xy;\n"
    "    \n"
    "    float dr = radius_1 - radius_0;\n"
    "    float dot_circle_1 = dot (circle_1, circle_1);\n"
    "    float dot_pos_circle_1 = dot (pos, circle_1);\n"
    "    \n"
    "    float A = dot_circle_1 - dr * dr;\n"
    "    float B = -2.0 * (dot_pos_circle_1 + radius_0 * dr);\n"
    "    float C = dot (pos, pos) - radius_0 * radius_0;\n"
    "    float det = B * B - 4.0 * A * C;\n"
    "    det = max (det, 0.0);\n"
    "    \n"
    "    float sqrt_det = sqrt (det);\n"
    "    /* This complicated bit of logic acts as\n"
    "     * \"if (A < 0.0) sqrt_det = -sqrt_det\", without the branch.\n"
    "     */\n"
    "    sqrt_det *= 1.0 + 2.0 * sign (min (A, 0.0));\n"
    "    \n"
    "    float t = (-B + sqrt_det) / (2.0 * A);\n"
    "    t = (t - first_offset) / (last_offset - first_offset);\n"
    "    gl_FragColor = texture1D (tex, t);\n"
    "}\n";

static const char * const linear_gradient_frag_text_110 =
    "#version 110\n"
    "\n"
    "uniform sampler1D tex;\n"
    "uniform mat4 matrix;\n"
    "uniform vec2 segment;\n"
    "uniform float first_offset;\n"
    "uniform float last_offset;\n"
    "\n"
    "void main ()\n"
    "{\n"
    "    vec2 pos = (matrix * vec4 (gl_FragCoord.xy, 0.0, 1.0)).xy;\n"
    "    float t = dot (pos, segment) / dot (segment, segment);\n"
    "    t = (t - first_offset) / (last_offset - first_offset);\n"
    "    gl_FragColor = texture1D (tex, t);\n"
    "}\n";

/* ARB_shader_objects / ARB_vertex_shader / ARB_fragment_shader extensions
   API. */
static cairo_status_t
compile_shader_arb (GLuint *shader, GLenum type, const char *text)
{
    const char* strings[1] = { text };
    GLint gl_status;

    *shader = glCreateShaderObjectARB (type);
    glShaderSourceARB (*shader, 1, strings, 0);
    glCompileShaderARB (*shader);
    glGetObjectParameterivARB (*shader, GL_OBJECT_COMPILE_STATUS_ARB, &gl_status);
    if (gl_status == GL_FALSE) {
        GLint log_size;
        glGetObjectParameterivARB (*shader, GL_OBJECT_INFO_LOG_LENGTH_ARB, &log_size);
        if (0 < log_size) {
            char *log = _cairo_malloc (log_size);
            GLint chars;

            log[log_size - 1] = '\0';
            glGetInfoLogARB (*shader, log_size, &chars, log);
            printf ("OpenGL shader compilation failed.  Shader:\n"
                    "%s\n"
                    "OpenGL compilation log:\n"
                    "%s\n",
                    text, log);

            free (log);
        } else {
            printf ("OpenGL shader compilation failed.\n");
        }

        return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
link_shader_arb (GLuint *program, GLuint vert, GLuint frag)
{
    GLint gl_status;

    *program = glCreateProgramObjectARB ();
    glAttachObjectARB (*program, vert);
    glAttachObjectARB (*program, frag);
    glLinkProgramARB (*program);
    glGetObjectParameterivARB (*program, GL_OBJECT_LINK_STATUS_ARB, &gl_status);
    if (gl_status == GL_FALSE) {
        GLint log_size;
        glGetObjectParameterivARB (*program, GL_OBJECT_INFO_LOG_LENGTH_ARB, &log_size);
        if (0 < log_size) {
            char *log = _cairo_malloc (log_size);
            GLint chars;

            log[log_size - 1] = '\0';
            glGetInfoLogARB (*program, log_size, &chars, log);
            printf ("OpenGL shader link failed:\n%s\n", log);

            free (log);
        } else {
            printf ("OpenGL shader link failed.\n");
        }

        return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
destroy_shader_program_arb (cairo_gl_shader_program_t *program)
{
    if (program->vertex_shader)
        glDeleteObjectARB (program->vertex_shader);
    if (program->fragment_shader)
        glDeleteObjectARB (program->fragment_shader);
    if (program->program)
        glDeleteObjectARB (program->program);
}

static cairo_status_t
create_linear_gradient_shader_program_arb (cairo_gl_shader_program_t *program)
{
    return create_shader_program (program,
                                  minimal_vert_text_110,
                                  linear_gradient_frag_text_110);
}

static cairo_status_t
create_radial_gradient_shader_program_arb (cairo_gl_shader_program_t *program)
{
    return create_shader_program (program,
                                  minimal_vert_text_110,
                                  radial_gradient_frag_text_110);
}

static cairo_status_t
bind_float_to_shader_arb (GLuint program, const char *name,
                               float value)
{
    GLint location = glGetUniformLocationARB (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform1fARB (location, value);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_vec2_to_shader_arb (GLuint program, const char *name,
                              float value0, float value1)
{
    GLint location = glGetUniformLocationARB (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform2fARB (location, value0, value1);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_vec3_to_shader_arb (GLuint program, const char *name,
                              float value0, float value1,
                              float value2)
{
    GLint location = glGetUniformLocationARB (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform3fARB (location, value0, value1, value2);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_vec4_to_shader_arb (GLuint program, const char *name,
                              float value0, float value1,
                              float value2, float value3)
{
    GLint location = glGetUniformLocationARB (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform4fARB (location, value0, value1, value2, value3);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_matrix_to_shader_arb (GLuint program, const char *name, cairo_matrix_t* m)
{
    GLint location = glGetUniformLocationARB (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    float gl_m[16] = {
        m->xx, m->xy, 0,     m->x0,
        m->yx, m->yy, 0,     m->y0,
        0,     0,     1,     0,
        0,     0,     0,     1
    };
    glUniformMatrix4fvARB (location, 1, GL_TRUE, gl_m);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_texture_to_shader_arb (GLuint program, const char *name, GLuint tex_unit)
{
    GLint location = glGetUniformLocationARB (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform1iARB (location, tex_unit);
    return CAIRO_STATUS_SUCCESS;
}

static GLenum
vertex_enumerator_arb (void)
{
    return GL_VERTEX_SHADER_ARB;
}

static GLenum
fragment_enumerator_arb (void)
{
    return GL_FRAGMENT_SHADER_ARB;
}

/* OpenGL Core 2.0 API. */
static cairo_status_t
compile_shader_core_2_0 (GLuint *shader, GLenum type, const char *text)
{
    const char* strings[1] = { text };
    GLint gl_status;

    *shader = glCreateShader (type);
    glShaderSource (*shader, 1, strings, 0);
    glCompileShader (*shader);
    glGetShaderiv (*shader, GL_COMPILE_STATUS, &gl_status);
    if (gl_status == GL_FALSE) {
        GLint log_size;
        glGetShaderiv (*shader, GL_INFO_LOG_LENGTH, &log_size);
        if (0 < log_size) {
            char *log = _cairo_malloc (log_size);
            GLint chars;

            log[log_size - 1] = '\0';
            glGetShaderInfoLog (*shader, log_size, &chars, log);
            printf ("OpenGL shader compilation failed.  Shader:\n"
                    "%s\n"
                    "OpenGL compilation log:\n"
                    "%s\n",
                    text, log);

            free (log);
        } else {
            printf ("OpenGL shader compilation failed.\n");
        }

        return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
link_shader_core_2_0 (GLuint *program, GLuint vert, GLuint frag)
{
    GLint gl_status;

    *program = glCreateProgram ();
    glAttachShader (*program, vert);
    glAttachShader (*program, frag);
    glLinkProgram (*program);
    glGetProgramiv (*program, GL_LINK_STATUS, &gl_status);
    if (gl_status == GL_FALSE) {
        GLint log_size;
        glGetProgramiv (*program, GL_INFO_LOG_LENGTH, &log_size);
        if (0 < log_size) {
            char *log = _cairo_malloc (log_size);
            GLint chars;

            log[log_size - 1] = '\0';
            glGetProgramInfoLog (*program, log_size, &chars, log);
            printf ("OpenGL shader link failed:\n%s\n", log);

            free (log);
        } else {
            printf ("OpenGL shader link failed.\n");
        }

        return CAIRO_INT_STATUS_UNSUPPORTED;
    }

    return CAIRO_STATUS_SUCCESS;
}

static void
destroy_shader_program_core_2_0 (cairo_gl_shader_program_t *program)
{
    glDeleteShader (program->vertex_shader);
    glDeleteShader (program->fragment_shader);
    glDeleteProgram (program->program);
}

static cairo_status_t
create_linear_gradient_shader_program_core_2_0 (cairo_gl_shader_program_t *program)
{
    return create_shader_program (program,
                                  minimal_vert_text_110,
                                  linear_gradient_frag_text_110);
}

static cairo_status_t
create_radial_gradient_shader_program_core_2_0 (cairo_gl_shader_program_t *program)
{
    return create_shader_program (program,
                                  minimal_vert_text_110,
                                  radial_gradient_frag_text_110);
}

static cairo_status_t
bind_float_to_shader_core_2_0 (GLuint program, const char *name,
                               float value)
{
    GLint location = glGetUniformLocation (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform1f (location, value);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_vec2_to_shader_core_2_0 (GLuint program, const char *name,
                              float value0, float value1)
{
    GLint location = glGetUniformLocation (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform2f (location, value0, value1);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_vec3_to_shader_core_2_0 (GLuint program, const char *name,
                              float value0, float value1,
                              float value2)
{
    GLint location = glGetUniformLocation (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform3f (location, value0, value1, value2);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_vec4_to_shader_core_2_0 (GLuint program, const char *name,
                              float value0, float value1,
                              float value2, float value3)
{
    GLint location = glGetUniformLocation (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform4f (location, value0, value1, value2, value3);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_matrix_to_shader_core_2_0 (GLuint program, const char *name, cairo_matrix_t* m)
{
    GLint location = glGetUniformLocation (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    float gl_m[16] = {
        m->xx, m->xy, 0,     m->x0,
        m->yx, m->yy, 0,     m->y0,
        0,     0,     1,     0,
        0,     0,     0,     1
    };
    glUniformMatrix4fv (location, 1, GL_TRUE, gl_m);
    return CAIRO_STATUS_SUCCESS;
}

static cairo_status_t
bind_texture_to_shader_core_2_0 (GLuint program, const char *name, GLuint tex_unit)
{
    GLint location = glGetUniformLocation (program, name);
    if (location == -1)
        return CAIRO_INT_STATUS_UNSUPPORTED;
    glUniform1i (location, tex_unit);
    return CAIRO_STATUS_SUCCESS;
}

static GLenum
vertex_enumerator_core_2_0 (void)
{
    return GL_VERTEX_SHADER;
}

static GLenum
fragment_enumerator_core_2_0 (void)
{
    return GL_FRAGMENT_SHADER;
}

static const shader_impl_t shader_impl_core_2_0 = {
    compile_shader_core_2_0,
    link_shader_core_2_0,
    destroy_shader_program_core_2_0,
    create_linear_gradient_shader_program_core_2_0,
    create_radial_gradient_shader_program_core_2_0,
    bind_float_to_shader_core_2_0,
    bind_vec2_to_shader_core_2_0,
    bind_vec3_to_shader_core_2_0,
    bind_vec4_to_shader_core_2_0,
    bind_matrix_to_shader_core_2_0,
    bind_texture_to_shader_core_2_0,
    vertex_enumerator_core_2_0,
    fragment_enumerator_core_2_0
};

static const shader_impl_t shader_impl_arb = {
    compile_shader_arb,
    link_shader_arb,
    destroy_shader_program_arb,
    create_linear_gradient_shader_program_arb,
    create_radial_gradient_shader_program_arb,
    bind_float_to_shader_arb,
    bind_vec2_to_shader_arb,
    bind_vec3_to_shader_arb,
    bind_vec4_to_shader_arb,
    bind_matrix_to_shader_arb,
    bind_texture_to_shader_arb,
    vertex_enumerator_arb,
    fragment_enumerator_arb
};

static const shader_impl_t*
get_impl (void)
{
    if (GLEW_VERSION_2_0) {
        return &shader_impl_core_2_0;
    } else if (GLEW_ARB_shader_objects &&
               GLEW_ARB_fragment_shader &&
               GLEW_ARB_vertex_program) {
        return &shader_impl_arb;
    }

    ASSERT_NOT_REACHED;
    return NULL;
}

void
init_shader_program (cairo_gl_shader_program_t *program)
{
    program->vertex_shader = 0;
    program->fragment_shader = 0;
    program->program = 0;
    program->build_failure = FALSE;
}

void
destroy_shader_program (cairo_gl_shader_program_t *program)
{
    return get_impl()->destroy_shader_program(program);
}

cairo_status_t
create_shader_program (cairo_gl_shader_program_t *program,
                       const char *vertex_text,
                       const char *fragment_text)
{
    cairo_status_t status;

    if (program->program != 0)
        return CAIRO_STATUS_SUCCESS;

    if (program->build_failure)
        return CAIRO_INT_STATUS_UNSUPPORTED;

    status = get_impl()->compile_shader (&program->vertex_shader,
                                         get_impl()->vertex_enumerator(),
                                         vertex_text);
    if (unlikely (status))
        goto FAILURE;

    status = get_impl()->compile_shader (&program->fragment_shader,
                                         get_impl()->fragment_enumerator(),
                                         fragment_text);
    if (unlikely (status))
        goto FAILURE;

    status = get_impl()->link_shader (&program->program,
                                      program->vertex_shader,
                                      program->fragment_shader);
    if (unlikely (status))
        goto FAILURE;

    return CAIRO_STATUS_SUCCESS;

 FAILURE:
    destroy_shader_program (program);
    program->vertex_shader = 0;
    program->fragment_shader = 0;
    program->program = 0;
    program->build_failure = TRUE;

    return CAIRO_INT_STATUS_UNSUPPORTED;
}

cairo_status_t
create_linear_gradient_shader_program (cairo_gl_shader_program_t *program)
{
    return get_impl()->create_linear_gradient_shader_program(program);
}

cairo_status_t
create_radial_gradient_shader_program (cairo_gl_shader_program_t *program)
{
    return get_impl()->create_radial_gradient_shader_program(program);
}

cairo_status_t
bind_float_to_shader (GLuint program, const char *name,
                      float value)
{
    return get_impl()->bind_float_to_shader(program, name, value);
}

cairo_status_t
bind_vec2_to_shader (GLuint program, const char *name,
                     float value0, float value1)
{
    return get_impl()->bind_vec2_to_shader(program, name, value0, value1);
}

cairo_status_t
bind_vec3_to_shader (GLuint program, const char *name,
                     float value0, float value1,
                     float value2)
{
    return get_impl()->bind_vec3_to_shader(program, name, value0, value1, value2);
}

cairo_status_t
bind_vec4_to_shader (GLuint program, const char *name,
                     float value0, float value1,
                     float value2, float value3)
{
    return get_impl()->bind_vec4_to_shader(program, name, value0, value1, value2, value3);
}

cairo_status_t
bind_matrix_to_shader (GLuint program, const char *name, cairo_matrix_t* m)
{
    return get_impl()->bind_matrix_to_shader(program, name, m);
}

cairo_status_t
bind_texture_to_shader (GLuint program, const char *name, GLuint tex_unit)
{
    return get_impl()->bind_texture_to_shader(program, name, tex_unit);
}
