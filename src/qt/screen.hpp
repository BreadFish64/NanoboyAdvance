/**
  * Copyright (C) 2017 flerovium^-^ (Frederic Meyer)
  *
  * This file is part of NanoboyAdvance.
  *
  * NanoboyAdvance is free software: you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation, either version 3 of the License, or
  * (at your option) any later version.
  *
  * NanoboyAdvance is distributed in the hope that it will be useful,
  * but WITHOUT ANY WARRANTY; without even the implied warranty of
  * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
  * GNU General Public License for more details.
  *
  * You should have received a copy of the GNU General Public License
  * along with NanoboyAdvance. If not, see <http://www.gnu.org/licenses/>.
  */

#pragma once

#include <unordered_map>

#include <QGLWidget>
#include "config.hpp"
#include "util/integer.hpp"

class Screen : public QGLWidget {
    Q_OBJECT

public:
     Screen(QtConfig* config, int width, int height, QWidget *parent = 0);
    ~Screen();

    void updateTexture();
    void compileShader(std::string shader_source, std::unordered_map<std::string, float>& uniforms);

    auto sizeHint() const -> QSize;

    void clear();
    auto pixels() const -> u32* { return framebuffer; }

    void aspectRatio(bool keep_ar) { aspect_ratio = keep_ar; }

protected:
    void initializeGL();

    void paintGL();
    void resizeGL(int width, int height);

private:
    int  width;
    int  height;
    u32* framebuffer;
    bool aspect_ratio;

    QtConfig* config;

    GLuint texture;
};
