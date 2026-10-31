/* dsfplatform shim for RageTextureManager.h.
 * RageDisplay_OGL only calls TEXTUREMAN->InvalidateTextures() after a GL context
 * is recreated.  The texture manager itself (bitmap/movie textures, caching) is
 * not part of the platform layer; the host renderer owns its textures and gets
 * the same notification through dsf::Config::pfnContextLost. */
#ifndef RAGE_TEXTURE_MANAGER_H
#define RAGE_TEXTURE_MANAGER_H

class RageTextureManager
{
public:
	void InvalidateTextures();
};

extern RageTextureManager* TEXTUREMAN;

#endif
