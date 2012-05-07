Simple 3D fluid simulation done purely in OpenGL by ping-ponging a layered framebuffer object / 3D texture.

At every frame, a deep shadow map is regenerated and a fragment shader performs raycasts against the 3D texture.

This code has been tested on CentOS 6 and RHEL 6, using a decent NVIDIA card and driver.  I probably won't have time to help you if you send me "it won't build on my platform" questions, but please feel free to fork and make pull requests.

You can re-use this code in any way, but I'd like you to give me attribution.  ([CC BY 3.0](http://creativecommons.org/licenses/by/3.0/))