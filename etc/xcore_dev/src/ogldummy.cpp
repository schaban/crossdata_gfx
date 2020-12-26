#include "oglsys.hpp"

namespace dummygl {

GLuint s_ibuf = 0;
GLuint s_ismp = 0;
GLuint s_itex = 0;
GLuint s_isdr = 0;
GLuint s_iprg = 0;
GLuint s_ifbo = 0;
GLuint s_irbo = 0;

void APIENTRY Viewport(GLint x, GLint y, GLsizei width, GLsizei height) {
}

void APIENTRY Scissor(GLint x, GLint y, GLsizei width, GLsizei height) {
}

void APIENTRY GetIntegerv(GLenum name, GLint* pData) {
	if (!pData) return;
	switch (name) {
		case GL_FRAMEBUFFER_BINDING:
			*pData = 0;
			break;
		case GL_MAX_TEXTURE_SIZE:
			*pData = 4096;
			break;
		default:
			*pData = 0;
			break;
	}
}

const GLubyte* APIENTRY GetString(GLenum name) {
	if (name == GL_VERSION) {
		return (GLubyte*)"dummygl 0.0";
	} else if (name == GL_VENDOR) {
		return (GLubyte*)"Kuwaraku Heavy Industries";
	} else if (name == GL_RENDERER) {
		return (GLubyte*)"blackhole renderer";
	} else if (name == GL_EXTENSIONS) {
		return (GLubyte*)"";
	}
	return (GLubyte*)"";
}

void APIENTRY LineWidth(GLfloat width) {
}

void APIENTRY ColorMask(GLboolean r, GLboolean g, GLboolean b, GLboolean a) {
}

void APIENTRY DepthMask(GLboolean flag) {
}

void APIENTRY DepthFunc(GLenum func) {
}

void APIENTRY StencilMask(GLuint mask) {
}

void APIENTRY StencilOp(GLenum fail, GLenum zfail, GLenum zpass) {
}

void APIENTRY StencilFunc(GLenum func, GLint ref, GLuint mask) {
}

void APIENTRY StencilMaskSeparate(GLenum face, GLuint mask) {
}

void APIENTRY StencilOpSeparate(GLenum face, GLenum sfail, GLenum dpfail, GLenum dppass) {
}

void APIENTRY StencilFuncSeparate(GLenum face, GLenum func, GLint ref, GLuint mask) {
}

void APIENTRY ClearColor(GLfloat red, GLfloat green, GLfloat blue, GLfloat alpha) {
}

void APIENTRY ClearDepth(GLdouble depth) {
}

void APIENTRY ClearStencil(GLint s) {
}

void APIENTRY Clear(GLbitfield mask) {
}

void APIENTRY Enable(GLenum cap) {
}

void APIENTRY Disable(GLenum cap) {
}

void APIENTRY FrontFace(GLenum mode) {
}

void APIENTRY CullFace(GLenum mode) {
}

void APIENTRY BlendEquation(GLenum mode) {
}

void APIENTRY BlendFunc(GLenum sfactor, GLenum dfactor) {
}

void APIENTRY BlendEquationSeparate(GLenum modeRGB, GLenum modeAlpha) {
}

void APIENTRY BlendFuncSeparate(GLenum sfactorRGB, GLenum dfactorRGB, GLenum sfactorAlpha, GLenum dfactorAlpha) {
}

void APIENTRY GenBuffers(GLsizei n, GLuint* pBuffers) {
	if (n > 0 && pBuffers) {
		for (GLsizei i = 0; i < n; ++i) {
			GLuint ibuf = s_ibuf++;
			if (ibuf == 0) ibuf = s_ibuf++;
			pBuffers[i] = ibuf;
		}
	}
}

void APIENTRY BindBuffer(GLenum target, GLuint buffer) {
}

void APIENTRY BufferData(GLenum target, GLsizeiptr size, const void* pData, GLenum usage) {
}

void APIENTRY BufferSubData(GLenum target, GLintptr offset, GLsizeiptr size, const void* pData) {
}

void APIENTRY BindBufferRange(GLenum target, GLuint index, GLuint buffer, GLintptr offset, GLsizeiptr size) {
}

void APIENTRY DeleteBuffers(GLsizei n, const GLuint* pBuffers) {
}

void APIENTRY GenSamplers(GLsizei n, GLuint* pSamplers) {
	if (n > 0 && pSamplers) {
		for (GLsizei i = 0; i < n; ++i) {
			GLuint ismp = s_ismp++;
			if (ismp == 0) ismp = s_ismp++;
			pSamplers[i] = ismp;
		}
	}
}

void APIENTRY DeleteSamplers(GLsizei count, const GLuint* pSamplers) {
}

void APIENTRY SamplerParameteri(GLuint sampler, GLenum pname, GLint param) {
}

void APIENTRY BindSampler(GLuint unit, GLuint sampler) {
}

void APIENTRY GenTextures(GLsizei n, GLuint* pTextures) {
	if (n > 0 && pTextures) {
		for (GLsizei i = 0; i < n; ++i) {
			GLuint itex = s_itex++;
			if (itex == 0) itex = s_itex++;
			pTextures[i] = itex;
		}
	}
}

void APIENTRY DeleteTextures(GLsizei n, const GLuint* pTextures) {
}

void APIENTRY BindTexture(GLenum target, GLuint texture) {
}

void APIENTRY TexImage2D(GLenum target, GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const void* pPixels) {
}

void APIENTRY TexStorage2D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height) {
}

void APIENTRY TexStorage3D(GLenum target, GLsizei levels, GLenum internalformat, GLsizei width, GLsizei height, GLsizei depth) {
}

void APIENTRY TexSubImage3D(GLenum target, GLint level, GLint xoffset, GLint yoffset, GLint zoffset, GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type, const void* pPixels) {
}

void APIENTRY TexParameteri(GLenum target, GLenum pname, GLint param) {
}

void APIENTRY ActiveTexture(GLenum texture) {
}

void APIENTRY GenerateMipmap(GLenum target) {
}

void APIENTRY PixelStorei(GLenum pname, GLint param) {
}

GLuint APIENTRY CreateShader(GLenum type) {
	GLuint isdr = 0;
	bool typeOk = type == GL_VERTEX_SHADER || type == GL_FRAGMENT_SHADER;
	if (typeOk) {
		isdr = s_isdr++;
		if (isdr == 0) isdr = s_isdr++;
	}
	return isdr;
}

void APIENTRY ShaderSource(GLuint shader, GLsizei count, const GLchar* const* ppString, const GLint* pLength) {
}

void APIENTRY CompileShader(GLuint shader) {
}

void APIENTRY DeleteShader(GLuint shader) {
}

void APIENTRY GetShaderiv(GLuint shader, GLenum pname, GLint* pParams) {
	if (shader && pParams) {
		switch (pname) {
			case GL_COMPILE_STATUS:
				*pParams = GL_TRUE;
				break;
			default:
				*pParams = 0;
				break;
		}
	} else if (pParams) {
		*pParams = 0;
	}
}

void APIENTRY GetShaderInfoLog(GLuint shader, GLsizei bufSize, GLsizei* pLength, GLchar* pInfoLog) {
	if (pLength) {
		*pLength = 0;
	}
}

void APIENTRY AttachShader(GLuint program, GLuint shader) {
}

void APIENTRY DetachShader(GLuint program, GLuint shader) {
}

GLuint APIENTRY CreateProgram() {
	GLuint iprg = s_iprg++;
	if (iprg == 0) iprg = s_iprg++;
	return iprg;
}

void APIENTRY LinkProgram(GLuint program) {
}

void APIENTRY DeleteProgram(GLuint program) {
}

void APIENTRY UseProgram(GLuint program) {
}

void APIENTRY GetProgramiv(GLuint program, GLenum pname, GLint* pParams) {
	if (program && pParams) {
		switch (pname) {
			case GL_LINK_STATUS:
				*pParams = GL_TRUE;
				break;
			default:
				*pParams = 0;
				break;
		}
	} else if (pParams) {
		*pParams = 0;
	}
}

void APIENTRY GetProgramInfoLog(GLuint program, GLsizei bufSize, GLsizei* pLength, GLchar* pInfoLog) {
	if (pLength) {
		*pLength = 0;
	}
}

GLint APIENTRY GetUniformLocation(GLuint program, const GLchar* pName) {
	GLint loc = 0;
	if (program) {
		loc = 1;
	}
	return loc;
}

void APIENTRY Uniform1i(GLint location, GLint v0) {
}

void APIENTRY Uniform1f(GLint location, GLfloat v0) {
}

void APIENTRY Uniform1fv(GLint location, GLsizei count, const GLfloat* pValue) {
}

void APIENTRY Uniform2fv(GLint location, GLsizei count, const GLfloat* pValue) {
}

void APIENTRY Uniform3fv(GLint location, GLsizei count, const GLfloat* pValue) {
}

void APIENTRY Uniform4fv(GLint location, GLsizei count, const GLfloat* pValue) {
}

void APIENTRY UniformMatrix4fv(GLint location, GLsizei count, GLboolean transpose, const GLfloat* pValue) {
}

GLint APIENTRY GetAttribLocation(GLuint program, const GLchar* name) {
	return 0;
}

GLuint APIENTRY GetUniformBlockIndex(GLuint program, const GLchar* pUniformBlockName) {
	GLuint idx = GL_INVALID_INDEX;
	if (program && pUniformBlockName) {
		idx = 0;
	}
	return idx;
}

void APIENTRY UniformBlockBinding(GLuint program, GLuint uniformBlockIndex, GLuint uniformBlockBinding) {
}

void APIENTRY VertexAttribPointer(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer) {
}

void APIENTRY EnableVertexAttribArray(GLuint index) {
}

void APIENTRY DisableVertexAttribArray(GLuint index) {
}

void APIENTRY VertexAttrib4fv(GLuint index, const GLfloat* pVal) {
}

void APIENTRY VertexAttrib4iv(GLuint index, const GLint* pVal) {
}

void APIENTRY BindBufferBase(GLenum target, GLuint index, GLuint buffer) {
}

void APIENTRY DrawArrays(GLenum mode, GLint first, GLsizei count) {
}

void APIENTRY DrawElements(GLenum mode, GLsizei count, GLenum type, const void* pIndices) {
}

void APIENTRY GenFramebuffers(GLsizei n, GLuint* pFramebuffers) {
	if (n > 0 && pFramebuffers) {
		for (GLsizei i = 0; i < n; ++i) {
			GLuint ifbo = s_ifbo++;
			if (ifbo == 0) ifbo = s_ifbo++;
			pFramebuffers[i] = ifbo;
		}
	}
}

void APIENTRY DeleteFramebuffers(GLsizei n, const GLuint* pFramebuffers) {
}

void APIENTRY BindFramebuffer(GLenum target, GLuint framebuffer) {
}

void APIENTRY FramebufferTexture2D(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level) {
}

void APIENTRY FramebufferRenderbuffer(GLenum target, GLenum attachment, GLenum renderbuffertarget, GLuint renderbuffer) {
}

GLenum APIENTRY CheckFramebufferStatus(GLenum target) {
	return GL_FRAMEBUFFER_COMPLETE;
}

void APIENTRY BlitFramebuffer(GLint srcX0, GLint srcY0, GLint srcX1, GLint srcY1, GLint dstX0, GLint dstY0, GLint dstX1, GLint dstY1, GLbitfield mask, GLenum filter) {
}

void APIENTRY GenRenderbuffers(GLsizei n, GLuint* pRenderbuffers) {
	if (n > 0 && pRenderbuffers) {
		for (GLsizei i = 0; i < n; ++i) {
			GLuint irbo = s_irbo++;
			if (irbo == 0) irbo = s_irbo++;
			pRenderbuffers[i] = irbo;
		}
	}
}

void APIENTRY DeleteRenderbuffers(GLsizei n, const GLuint* pRenderbuffers) {
}

void APIENTRY BindRenderbuffer(GLenum target, GLuint renderbuffer) {
}

void APIENTRY RenderbufferStorage(GLenum target, GLenum internalformat, GLsizei width, GLsizei height) {
}

GLenum APIENTRY GetError() {
	return GL_NO_ERROR;
}

} // dummygl

#undef OGL_FN_CORE
#define OGL_FN_CORE
#undef OGL_FN_EXTRA
#define OGL_FN(_type, _name) gl##_name = dummygl::_name;

#if defined(_MSC_VER)
__declspec(noinline)
#elif defined(__GNUC__)
__attribute__((noinline))
#endif
void dummyglInit() {
#		include "oglsys.inc"
}

#undef OGL_FN
