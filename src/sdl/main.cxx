/*
    Tickle 0.95
    Main for SDL version
    Copyright (c) 2014-2021 Alessandro Scotti
*/

#include <limits.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <map>
#include "SDL2/SDL.h"
#include "emu/emu.h"
#include "sdl_main.h"

char * basePath;

#ifdef WIN32
const wchar_t PATH_SEPARATOR = '\\';
#else
const wchar_t PATH_SEPARATOR = '/';
#endif

// For some reasons g++ may get confused by the indirect reference and will not link the machines
// (this happens e.g. on the RPI) so we have to include them explicitly.
#include <machine/1942.h>
#include <machine/gauntlet.h>
#include <machine/fantasy.h>
#include <machine/frogger.h>
#include <machine/galaga.h>
#include <machine/galaxian.h>
#include <machine/invaders.h>
#include <machine/nibbler.h>
#include <machine/pacman.h>
#include <machine/pengo.h>
#include <machine/pinball_action.h>
#include <machine/pooyan.h>
#include <machine/rallyx.h>
#include <machine/scramble.h>
#include <machine/vanguard.h>
#include <machine/spectrum.h>

void dummy()
{
    delete M1942::createInstance();
    delete Gauntlet::createInstance();
    delete Fantasy::createInstance();
    delete Frogger::createInstance();
    delete Galaga::createInstance();
    delete Galaxian::createInstance();
    delete Nibbler::createInstance();
    delete Pacman::createInstance();
    delete Pengo::createInstance();
    delete PinballAction::createInstance();
    delete Pooyan::createInstance();
    delete RallyX::createInstance();
    delete Scramble::createInstance();
    delete SpaceInvaders::createInstance();
    delete Vanguard::createInstance();
    delete ZX48k::createInstance();
}

// Load a file from an input stream into a newly allocated buffer
// (which must be deleted by the caller)
unsigned char * loadFileFromStream( TInputStream * is, unsigned size, TCRC32 * crc )
{
    unsigned char * result = new unsigned char [size];
    if( is->read( result, size ) == size ) {
        crc->reset();
        crc->update( result, size );
    }
    else {
        delete [] result;
        result = 0;
    }
    return result;
}

unsigned getFileSize( const char * name )
{
    unsigned result = 0;
    FILE * f = fopen( name, "rb" );
    if( f != 0 ) {
        fseek( f, 0L, SEEK_END );
        result = (unsigned) ftell( f );
        fclose( f );
    }
    return result;
}

unsigned char * loadRawFile( const char * name, unsigned * size )
{
    unsigned char * result = 0;
    *size = 0;

    FILE * f = fopen( name, "rb" );
    if( f != 0 ) {
        fseek( f, 0L, SEEK_END );
        long s = ftell( f );
        fseek( f, 0L, SEEK_SET );

        if( s > 0 ) {
            result = new unsigned char[(unsigned)s];
            if( fread( result, 1, (unsigned)s, f ) == (unsigned)s ) {
                *size = (unsigned)s;
            }
            else {
                delete [] result;
                result = 0;
            }
        }

        fclose( f );
    }

    return result;
}

// Load a single file required by the driver
unsigned char * loadFile( TMachine * machine, const TResourceFileInfo * info, const char * base, unsigned * bufsize )
{
    unsigned char * result = 0;
    TString home( basePath );
    if( home.wstr()[home.length()-1] != PATH_SEPARATOR ) {
        home.append(PATH_SEPARATOR);
    }
    if( base != 0 ) {
        home += base;
        home.append(PATH_SEPARATOR);
    }

    TString file = home + info->name;
    TCRC32 crc;
    TInputStream * is = TFileInputStream::open( file.cstr() );
    if( is != 0 ) {
        unsigned size = info->size;
        if( size == 0 ) {
            size = getFileSize( file.cstr() );
        }
        result = loadFileFromStream( is, size, &crc );
        *bufsize = size;
        delete is;
    }
    else {
        for( int i=0; i<machine->getResourceCount(); i++ ) {
            file = home + machine->getResourceName(i) + ".zip";
            TZipFile * zf = TZipFile::open( file.cstr() );
            if( zf != 0 ) {
                const TZipEntry * ze = zf->entry( info->name, true );
                if( ze != 0 ) {
                    is = ze->open();
                    if( is != 0 ) {
                        *bufsize = info->size ? info->size : ze->size();
                        result = loadFileFromStream( is, *bufsize, &crc );
                    }
                    delete is;
                }
                delete zf;
            }
            if( result != 0 ) {
                break;
            }
        }
    }

    if( result != 0 ) {
        // Delete buffer if CRC check fails
        if( (info->crc != 0) && (crc.value() != info->crc) ) {
            delete [] result;
            result = 0;
        }
    }

    return result;
}

// Load the files required by the driver
bool loadMachineFiles( TMachine * machine, TList & failedList )
{
    bool result = true;
    const TMachineDriverInfo * info = machine->getDriverInfo();

    for( int i=0; i<info->resourceFileCount(); i++ ) {
        const TResourceFileInfo * file = info->resourceFile( i );
        unsigned size = 0;

        unsigned char * buf = loadFile( machine, file, "roms", &size );
        if( (buf == 0) && (strstr(file->name,".wav") != 0) ) {
            buf = loadFile( machine, file, "samples", &size );
        }

        if( buf != 0 ) {
            machine->setResourceFile( file->id, buf, size );
            delete [] buf;
        }
        else {
            failedList.add( (void *)file->name );
            result = false;
        }
    }

    return result;
}

TMachine * loadGame( const char * name )
{
    TMachine * m = 0;

    int index = TGameRegistry::instance().find( name );
    if( index < 0 ) {
        printf( "The specified driver was not found\n" );
    }

    const TGameRegistryItem * t = TGameRegistry::instance().item( index );
    if( t != 0 ) {
        m = TMachine::createInstance( t->factory() );
    }

    // Load machine
    if( m != 0 ) {
        TList badFiles;
        bool ok = loadMachineFiles( m, badFiles );

        if( ! ok ) {
            TString msg = "One or more files could not be loaded:\n";
            for( int i=0; i<badFiles.count(); i++ ) {
                msg += "- ";
                msg += (char *) badFiles.item(i);
                msg += "\n";
            }

            msg += "\nFiles were looked for in the \"roms\" folder (also \"samples\""
                   "\nfor sounds) and in the following archives:\n";
            for( int j=0; j<m->getResourceCount(); j++ ) {
                msg += "- ";
                msg += m->getResourceName( j );
                msg += ".zip\n";
            }

            printf( "%s\n", msg.cstr() );

            if( ! ok ) {
                delete m;
                m = 0;
            }
        }
    }

    return m;
}

static void setupArcadeInputMappings( TEmuInputManager & inputManager, TJoystick * joy[4] )
{
    inputManager.add( SDLK_LCTRL, idKeyP1Action1 );
    inputManager.add( SDLK_SPACE, idKeyP1Action1 );
    inputManager.add( SDLK_z, idKeyP1Action2 ); // Z
    inputManager.add( SDLK_x, idKeyP1Action3 ); // X
    inputManager.add( SDLK_c, idKeyP1Action4 ); // C
    inputManager.add( SDLK_e, idKeyP2Action1 ); // E
    inputManager.add( SDLK_r, idKeyP2Action2 ); // R
    inputManager.add( SDLK_t, idKeyP2Action3 ); // T
    inputManager.add( SDLK_g, idKeyP2Action4 ); // G
    inputManager.add( SDLK_1, idKeyStartPlayer1 ); // 1
    inputManager.add( SDLK_2, idKeyStartPlayer2 ); // 2
    inputManager.add( SDLK_5, idCoinSlot1 ); // 5
    inputManager.add( SDLK_6, idCoinSlot2 ); // 6
    inputManager.add( SDLK_0, idKeyService1 ); // 0

    // Joysticks for player 1
    joy[0] = inputManager.addJoystick( idJoyP1Joystick1, SDLK_LEFT, SDLK_RIGHT, SDLK_UP, SDLK_DOWN );
    joy[0]->bindButtonToKey( 0, idKeyP1Action1 );
    joy[0]->bindButtonToKey( 1, idKeyP1Action2 );
    joy[0]->bindButtonToKey( 2, idKeyP1Action3 );
    joy[0]->bindButtonToKey( 3, idKeyP1Action4 );
    joy[0]->bindButtonToKey( 4, idKeyP1Action1 );
    joy[0]->bindButtonToKey( 5, idKeyP1Action2 );
    joy[0]->bindButtonToKey( 6, idKeyStartPlayer1 );
    joy[0]->bindButtonToKey( 7, idCoinSlot1 );

    joy[2] = inputManager.addJoystick( idJoyP1Joystick2, SDLK_a, SDLK_d, SDLK_w, SDLK_s );

    // Joysticks for player 2
    joy[1] = inputManager.addJoystick( idJoyP2Joystick1, SDLK_j, SDLK_l, SDLK_i, SDLK_k );
    joy[1]->bindButtonToKey( 0, idKeyP2Action1 );
    joy[1]->bindButtonToKey( 1, idKeyP2Action2 );
    joy[1]->bindButtonToKey( 2, idKeyP2Action3 );
    joy[1]->bindButtonToKey( 3, idKeyP2Action4 );
    joy[1]->bindButtonToKey( 6, idKeyStartPlayer2 );
    joy[1]->bindButtonToKey( 7, idCoinSlot1 );
}

static void setupSpectrumInputMappings( TEmuInputManager & inputManager )
{
    // row 0: CAPS SHIFT, Z, X, C, V
    inputManager.add( SDLK_LSHIFT,     idSpectrumKey, ZX48K_KEY_DATA(0,0) );
    inputManager.add( SDLK_RSHIFT,     idSpectrumKey, ZX48K_KEY_DATA(0,0) );
    inputManager.add( SDLK_z,          idSpectrumKey, ZX48K_KEY_DATA(0,1) );
    inputManager.add( SDLK_x,          idSpectrumKey, ZX48K_KEY_DATA(0,2) );
    inputManager.add( SDLK_c,          idSpectrumKey, ZX48K_KEY_DATA(0,3) );
    inputManager.add( SDLK_v,          idSpectrumKey, ZX48K_KEY_DATA(0,4) );

    // row 1: A, S, D, F, G
    inputManager.add( SDLK_a,          idSpectrumKey, ZX48K_KEY_DATA(1,0) );
    inputManager.add( SDLK_s,          idSpectrumKey, ZX48K_KEY_DATA(1,1) );
    inputManager.add( SDLK_d,          idSpectrumKey, ZX48K_KEY_DATA(1,2) );
    inputManager.add( SDLK_f,          idSpectrumKey, ZX48K_KEY_DATA(1,3) );
    inputManager.add( SDLK_g,          idSpectrumKey, ZX48K_KEY_DATA(1,4) );

    // row 2: Q, W, E, R, T
    inputManager.add( SDLK_q,          idSpectrumKey, ZX48K_KEY_DATA(2,0) );
    inputManager.add( SDLK_w,          idSpectrumKey, ZX48K_KEY_DATA(2,1) );
    inputManager.add( SDLK_e,          idSpectrumKey, ZX48K_KEY_DATA(2,2) );
    inputManager.add( SDLK_r,          idSpectrumKey, ZX48K_KEY_DATA(2,3) );
    inputManager.add( SDLK_t,          idSpectrumKey, ZX48K_KEY_DATA(2,4) );

    // row 3: 1, 2, 3, 4, 5
    inputManager.add( SDLK_1,          idSpectrumKey, ZX48K_KEY_DATA(3,0) );
    inputManager.add( SDLK_2,          idSpectrumKey, ZX48K_KEY_DATA(3,1) );
    inputManager.add( SDLK_3,          idSpectrumKey, ZX48K_KEY_DATA(3,2) );
    inputManager.add( SDLK_4,          idSpectrumKey, ZX48K_KEY_DATA(3,3) );
    inputManager.add( SDLK_5,          idSpectrumKey, ZX48K_KEY_DATA(3,4) );

    // row 4: 0, 9, 8, 7, 6
    inputManager.add( SDLK_0,          idSpectrumKey, ZX48K_KEY_DATA(4,0) );
    inputManager.add( SDLK_9,          idSpectrumKey, ZX48K_KEY_DATA(4,1) );
    inputManager.add( SDLK_8,          idSpectrumKey, ZX48K_KEY_DATA(4,2) );
    inputManager.add( SDLK_7,          idSpectrumKey, ZX48K_KEY_DATA(4,3) );
    inputManager.add( SDLK_6,          idSpectrumKey, ZX48K_KEY_DATA(4,4) );

    // row 5: P, O, I, U, Y
    inputManager.add( SDLK_p,          idSpectrumKey, ZX48K_KEY_DATA(5,0) );
    inputManager.add( SDLK_o,          idSpectrumKey, ZX48K_KEY_DATA(5,1) );
    inputManager.add( SDLK_i,          idSpectrumKey, ZX48K_KEY_DATA(5,2) );
    inputManager.add( SDLK_u,          idSpectrumKey, ZX48K_KEY_DATA(5,3) );
    inputManager.add( SDLK_y,          idSpectrumKey, ZX48K_KEY_DATA(5,4) );

    // row 6: ENTER, L, K, J, H
    inputManager.add( SDLK_RETURN,     idSpectrumKey, ZX48K_KEY_DATA(6,0) );
    inputManager.add( SDLK_l,          idSpectrumKey, ZX48K_KEY_DATA(6,1) );
    inputManager.add( SDLK_k,          idSpectrumKey, ZX48K_KEY_DATA(6,2) );
    inputManager.add( SDLK_j,          idSpectrumKey, ZX48K_KEY_DATA(6,3) );
    inputManager.add( SDLK_h,          idSpectrumKey, ZX48K_KEY_DATA(6,4) );

    // row 7: SPACE, SYMBOL SHIFT, M, N, B
    inputManager.add( SDLK_SPACE,      idSpectrumKey, ZX48K_KEY_DATA(7,0) );
    inputManager.add( SDLK_RALT,       idSpectrumKey, ZX48K_KEY_DATA(7,1) );
    inputManager.add( SDLK_RCTRL,      idSpectrumKey, ZX48K_KEY_DATA(7,1) );
    inputManager.add( SDLK_m,          idSpectrumKey, ZX48K_KEY_DATA(7,2) );
    inputManager.add( SDLK_n,          idSpectrumKey, ZX48K_KEY_DATA(7,3) );
    inputManager.add( SDLK_b,          idSpectrumKey, ZX48K_KEY_DATA(7,4) );

    // BACKSPACE -> CAPS SHIFT + 0 (DELETE)
    inputManager.add( SDLK_BACKSPACE,  idSpectrumKeyCombo, ZX48K_KEYCOMBO_DATA(0,0,4,0) );

    // Cursors:
    // Left  = CAPS SHIFT + 5
    // Down  = CAPS SHIFT + 6
    // Up    = CAPS SHIFT + 7
    // Right = CAPS SHIFT + 8
    inputManager.add( SDLK_LEFT,       idSpectrumKeyCombo, ZX48K_KEYCOMBO_DATA(0,0,3,4) );
    inputManager.add( SDLK_DOWN,       idSpectrumKeyCombo, ZX48K_KEYCOMBO_DATA(0,0,4,4) );
    inputManager.add( SDLK_UP,         idSpectrumKeyCombo, ZX48K_KEYCOMBO_DATA(0,0,4,3) );
    inputManager.add( SDLK_RIGHT,      idSpectrumKeyCombo, ZX48K_KEYCOMBO_DATA(0,0,4,2) );
}

int main(int argc, char** argv) {
    printf( "Tickle 0.95\n" );

    // Initialize base directory information
    basePath = (char *) malloc(PATH_MAX+1);
    getcwd(basePath, PATH_MAX+1);

    // Load game
    TMachine * machine = 0;
    TEmuInputManager inputManager;
    TJoystick * joy[4] = { 0, 0, 0, 0 };
    SDLMainOptions options;

    const char * driver = 0;
    const char * snaPath = 0;
    const char * tapPath = 0;
    int scale = 1; // default x2

    for( int i=1; i<argc; i++ ) {
        const char * a = argv[i];

        if( ! strcmp(a,"-list") ) {
            TGameRegistry & reg = TGameRegistry::instance();
            reg.sort();
            for( int i=0; i<reg.count(); i++ ) {
                const TGameRegistryItem * t = reg.item( i );
                printf( "%s (%s)\n", t->info()->driver, t->name() );
            }
            return EXIT_SUCCESS;
        }
        else if( ! strcmp(a,"-help") || ! strcmp(a,"-?") ) {
            printf( "-fs          fullscreen mode (default is windowed)\n" );
            printf( "--scale=x    window scale factor (x1, x2, x3, ...)\n" );
            printf( "--sna=file   load ZX Spectrum 48K .sna snapshot after ROM load\n" );
            printf( "--tap=file   load ZX Spectrum .tap tape image\n" );
            printf( "-list        list available drivers\n" );
            return EXIT_SUCCESS;
        }
        else if( ! strcmp(a,"-fs") ) {
            options.fullscreen = true;
        }
        else if( ! strncmp(a, "--scale=", 8) ) {
            int s = atoi( a + 8 );
            if( s >= 1 ) {
                scale = s;
            }
            else {
                printf( "Invalid scale value: %s\n", a + 8 );
                printf( "Scale must be >= 1\n" );
                return EXIT_FAILURE;
            }
        }
        else if( ! strncmp(a, "--sna=", 6) ) {
            snaPath = a + 6;
        }
        else if( ! strncmp(a, "--tap=", 6) ) {
            tapPath = a + 6;
        }
        else {
            driver = a;
        }
    }

    if( driver ) {
        machine = loadGame( driver );
        if( ! machine ) {
            printf( "Cannot load driver: %s\n", driver );
            printf( "(Use the -list option to get a list of drivers)\n");
            return EXIT_FAILURE;
        }
    }
    else {
        printf( "No driver specified: entering logo mode. Press LCTRL+1+5 to enter test mode.\n" );
        printf( "(Use the -list option to get a list of drivers)\n");
        machine = TTickleMachine::create();
    }

    // Optional external SNA load
    if( snaPath != 0 ) {
        const TMachineDriverInfo * mdi = machine->getDriverInfo();
        const char * loadedDriver = mdi->machineInfo()->driver;

        if( loadedDriver == 0 || strcmp( loadedDriver, "zx48k" ) != 0 ) {
            printf( "--sna is only supported with the zx48k driver\n" );
            delete machine;
            return EXIT_FAILURE;
        }

        unsigned snaSize = 0;
        unsigned char * snaBuf = loadRawFile( snaPath, &snaSize );
        if( snaBuf == 0 ) {
            printf( "Cannot load SNA file: %s\n", snaPath );
            delete machine;
            return EXIT_FAILURE;
        }

        if( ! machine->setResourceFile( EfSpectrumSnapshot, snaBuf, snaSize ) ) {
            printf( "Invalid or unsupported SNA file: %s\n", snaPath );
            delete [] snaBuf;
            delete machine;
            return EXIT_FAILURE;
        }

        delete [] snaBuf;
    }

    // Optional external TAP load
    if( tapPath != 0 ) {
        const TMachineDriverInfo * mdi = machine->getDriverInfo();
        const char * loadedDriver = mdi->machineInfo()->driver;

        if( loadedDriver == 0 || strcmp( loadedDriver, "zx48k" ) != 0 ) {
            printf( "--tap is only supported with the zx48k driver\n" );
            delete machine;
            return EXIT_FAILURE;
        }

        unsigned tapSize = 0;
        unsigned char * tapBuf = loadRawFile( tapPath, &tapSize );
        if( tapBuf == 0 ) {
            printf( "Cannot load TAP file: %s\n", tapPath );
            delete machine;
            return EXIT_FAILURE;
        }

        if( ! machine->setResourceFile( EfSpectrumTape, tapBuf, tapSize ) ) {
            printf( "Invalid or unsupported TAP file: %s\n", tapPath );
            delete [] tapBuf;
            delete machine;
            return EXIT_FAILURE;
        }

        delete [] tapBuf;
    }

    // Setup emulator input mappings
    const TMachineDriverInfo * mdi = machine->getDriverInfo();
    const char * loadedDriver = mdi->machineInfo()->driver;

    if( loadedDriver != 0 && ! strcmp( loadedDriver, "zx48k" ) ) {
        setupSpectrumInputMappings( inputManager );
    }
    else {
        setupArcadeInputMappings( inputManager, joy );
    }

    // Initialize SDL
    const TMachineDriverInfo * info = machine->getDriverInfo();
    options.w = scale * info->machineInfo()->screenWidth;
    options.h = scale * info->machineInfo()->screenHeight;

    SDLMain sdl;
    if( ! sdl.init( options ) ) {
        delete machine;
        return EXIT_FAILURE;
    }

    bool running;
    bool paused = false;
    bool turbo = false;

    running = sdl.go( machine );

    // Input latch: keep each pressed input active for a minimum number of
    // emulated frames so a brief key tap (e.g. inserting a coin or pressing
    // start) is always seen by the game, even if the key is pressed and
    // released within a single frame.
    const int InputLatchFrames = 4;
    std::map<int,int>  inputLatch;  // keysym -> frames left to stay pressed
    std::map<int,bool> inputHeld;   // keysym -> physically held down

    // Event loop
    while( running ) {
        SDL_Event e;

        while( SDL_PollEvent(&e) ) {
            switch( e.type ) {
                case SDL_USEREVENT:
                    switch( e.user.code ) {
                        case SDLTickleEvent_AddFrame:
                            // Update joystick status: for now, only 2 joysticks are supported
                            for( int i=0; i<2; i++ ) {
                                if( joy[i] != 0 ) {
                                    Sint16 x;
                                    Sint16 y;
                                    unsigned buttons;
                                    if( sdl.joystick_status( i, &x, &y, &buttons ) ) {
                                        joy[i]->setPosition( x, y );
                                        joy[i]->setButtons( buttons );
                                    }
                                }
                            }

                            inputManager.notifyJoysticks( machine );
                            sdl.add_frame( machine );

                            // Expire input latches: release keys whose latch
                            // has counted down and are no longer held.
                            for( std::map<int,int>::iterator it = inputLatch.begin(); it != inputLatch.end(); ) {
                                if( it->second > 0 ) it->second--;
                                if( it->second == 0 && ! inputHeld[it->first] ) {
                                    inputManager.handle( it->first, 0, machine );
                                    inputLatch.erase( it++ );
                                }
                                else {
                                    ++it;
                                }
                            }
                            break;

                        case SDLTickleEvent_RenderTexture:
                            sdl.render( (SDL_Texture *) e.user.data1 );
                            break;

                        case SDLTickleEvent_DestroyTexture:
                            SDL_DestroyTexture( (SDL_Texture *) e.user.data1 );
                            break;
                    }
                    break;

                case SDL_QUIT:
                    running = false;
                    break;

                case SDL_KEYUP:
                    inputHeld[e.key.keysym.sym] = false;
                    // If the key is still within its minimum-hold latch window,
                    // defer the release until the latch expires (handled after
                    // add_frame); otherwise release it now.
                    if( inputLatch.find( e.key.keysym.sym ) == inputLatch.end() ) {
                        inputManager.handle( e.key.keysym.sym, 0, machine );
                    }
                    break;

                case SDL_KEYDOWN:
                    if( inputManager.handle( e.key.keysym.sym, 1, machine ) ) {
                        inputHeld[e.key.keysym.sym] = true;
                        inputLatch[e.key.keysym.sym] = InputLatchFrames;
                    }
                    else {
                        // Unhandled key
                        switch( e.key.keysym.sym ) {
                            case SDLK_ESCAPE:
                                running = false;
                                break;

                            case SDLK_p:
                                paused = ! paused;
                                if( paused ) {
                                    sdl.audio_stop();
                                }
                                else {
                                    sdl.audio_play();
                                }
                                break;

                            case SDLK_F11:
                                if( machine->handleInputEvent( idSpectrumTapeControl, 1, 0 ) ) {
                                    printf( "Tape: REWIND + PLAY\n" );
                                }
                                else {
                                    printf( "Tape control ignored (no TAP loaded or non-zx48k driver)\n" );
                                }
                                break;

                            case SDLK_F12:
                                turbo = ! turbo;
                                printf( "Turbo mode: %s\n", turbo ? "ON" : "OFF" );
                                break;
                        
                        }
                    }
                    break;

                default:
                    break;
            }
        }

        // Turbo mode: process extra frames as fast as possible
        if( turbo && ! paused ) {
            for( int i=0; i<8; i++ ) {
                inputManager.notifyJoysticks( machine );
                sdl.add_frame( machine );
            }
        }
    }

    delete machine;
    return EXIT_SUCCESS;
}