#include <glcore/scoped_bind.hpp>

namespace glc {

ScopedBind::ScopedBind(const VertexArray& vao) noexcept : kind_{Kind::Vao} {
	glBindVertexArray(vao.id());
}

ScopedBind::ScopedBind(GLenum target, const Buffer& buffer) noexcept
    : kind_{Kind::Buffer}, target_{target} {
	glBindBuffer(target_, buffer.id());
}

ScopedBind::ScopedBind(GLenum target, const Texture& texture, GLuint unit) noexcept
    : kind_{Kind::Texture}, target_{target}, unit_{unit} {
	glActiveTexture(GL_TEXTURE0 + unit_);
	glBindTexture(target_, texture.id());
}

ScopedBind::~ScopedBind() {
	switch (kind_) {
		case Kind::Vao:
			glBindVertexArray(0);
			break;
		case Kind::Buffer:
			glBindBuffer(target_, 0);
			break;
		case Kind::Texture:
			glActiveTexture(GL_TEXTURE0 + unit_);
			glBindTexture(target_, 0);
			break;
	}
}

} // namespace glc
