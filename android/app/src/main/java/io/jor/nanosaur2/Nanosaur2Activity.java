package io.jor.nanosaur2;

import org.libsdl.app.SDLActivity;

public class Nanosaur2Activity extends SDLActivity {
    @Override
    protected String[] getLibraries() {
        return new String[] { "SDL3", "main" };
    }

    @Override
    protected String getMainFunction() {
        return "SDL_main";
    }
}
