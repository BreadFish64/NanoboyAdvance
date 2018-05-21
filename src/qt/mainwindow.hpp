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

#include <QMainWindow>
#include <QMenuBar>
#include <QStatusBar>
#include <QLabel>
#include <QTimer>

#include "screen.hpp"
#include "config.hpp"
#include "core/system/gba/emulator.hpp"

#include "util/ini.hpp"

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
     MainWindow(QApplication* app, QWidget* parent = 0);
    ~MainWindow();

public slots:
    void showOpenFileDialog();

    void nextFrame();

protected:
    bool eventFilter(QObject* obj, QEvent* event);

private:
    enum class EmulationState {
        Stopped,
        Running,
        Paused
    };

    void setupMenu();
    void setupFileMenu();
    void setupHelpMenu();
    void setupEmulationMenu();
    void setupEmuTimers();
    void setupStatusBar();

    void resetUI();
    void applyConfiguration();

    void runGame(const QString& rom_file);
    void pauseClicked();
    void stopClicked();

    // Setup SDL2 Audio Subsystem
    void setupSound(Core::APU* apu);
    void closeAudio();

    // Called by SDL2 to request new audio
    static void soundCallback(void* _apu, u8* _stream, int length);

    static auto qtKeyToEmu(int key) -> Core::Key;

    Screen* screen;

    QTimer timer_run { this };
    QTimer timer_fps { this };

    // Program Status
    QLabel     status_msg { this };
    QStatusBar status_bar { this };

    // Menu structure
    struct {
        QMenuBar* bar;
        struct {
            QMenu* menu;

            QAction* open;
            QAction* close;
        } file;
        struct {
            QMenu* menu;

            QAction* reset;
            QAction* pause;
            QAction* stop;
        } emulation;
        struct {
            QMenu* menu;

            QAction* qt;
            QAction* app;
        } help;
    } menubar;

    int frames = 0;

    // Keep track of emulation state: Running/Stopped/Paused
    EmulationState emu_state = EmulationState::Stopped;

    Util::INI ini { "config.ini", true };

    // Emulator instance
    QtConfig*       config   = nullptr;
    Core::Emulator* emulator = nullptr;
};
