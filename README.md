Simple 3D fluid simulation done purely in OpenGL by ping-ponging a layered framebuffer object / 3D texture.

At every frame, a deep shadow map is regenerated and a fragment shader performs raycasts against the 3D texture.

You can re-use this code in any way, but I'd like you to give me attribution.  ([CC BY 3.0](http://creativecommons.org/licenses/by/3.0/))