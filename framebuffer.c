/* See Copyright Notice in LICENSE.txt */

#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

#include <GL/glew.h>
#include <GL/gl.h>

#include "utlist.h"
#include "misc.h"

#define MAX_CACHED 30

typedef struct framebuffer {
    GLuint fbo;
    GLuint tex;
    int width;
    int height;
    struct framebuffer *prev;
    struct framebuffer *next;
} framebuffer_t;

static framebuffer_t *framebuffers = NULL;
static int num_framebuffers = 0;

static void unlink_framebuffer(framebuffer_t *framebuffer) {
    DL_DELETE(framebuffers, framebuffer);
    free(framebuffer);
    num_framebuffers--;
}

void make_framebuffer(int width, int height, GLuint *tex, GLuint *fbo) {
    framebuffer_t *framebuffer, *tmp;
    GLuint depth;

    DL_FOREACH_SAFE(framebuffers, framebuffer, tmp) {
        // Same size?
        if (framebuffer->height == height && framebuffer->width == width) {
            *tex = framebuffer->tex;
            *fbo = framebuffer->fbo;
            glBindFramebuffer(GL_FRAMEBUFFER, framebuffer->fbo);
            glBindTexture(GL_TEXTURE_2D, framebuffer->tex);
            unlink_framebuffer(framebuffer);
            return;
        }
    }

    glGenFramebuffers(1, fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, *fbo);
    fprintf(stderr, INFO("new framebuffer (%dx%d): %u\n"), width, height, *fbo);

    glGenTextures(1, tex);
    glBindTexture(GL_TEXTURE_2D, *tex);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_INT, NULL);

    glGenTextures(1, &depth);
    glBindTexture(GL_TEXTURE_2D, depth);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, width, height, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);

    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, *tex, 0);
    glFramebufferTexture2DEXT(GL_FRAMEBUFFER_EXT, GL_DEPTH_ATTACHMENT_EXT, GL_TEXTURE_2D, depth, 0);
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        die("cannot initialize new framebuffer");
}

void recycle_framebuffer(int width, int height, GLuint tex, GLuint fbo) {
    framebuffer_t *framebuffer = xmalloc(sizeof(framebuffer_t));
    framebuffer->width = width;
    framebuffer->height = height;
    framebuffer->tex = tex;
    framebuffer->fbo = fbo;

    // fprintf(stderr, "added recyleable framebuffer %dx%d %d %d\n", framebuffer->width, framebuffer->height,
    //     framebuffer->tex, framebuffer->fbo);

    DL_APPEND(framebuffers, framebuffer);
    num_framebuffers++;

    if (num_framebuffers > MAX_CACHED) {
        fprintf(stderr, ERROR("too many framebuffers in use\n"));
        glDeleteFramebuffers(1, &framebuffers->fbo);
        glDeleteTextures(1, &framebuffers->tex);
        unlink_framebuffer(framebuffers);
    }
}
