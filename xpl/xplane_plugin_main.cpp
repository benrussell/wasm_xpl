
//FIXME: xp_api seems to depend on these..
#include <XPLMPlugin.h>
#include <XPLMProcessing.h>
#include <XPLMUtilities.h>
#include <XPLMPlanes.h>
#include <XPLMDisplay.h>
#include <GL/gl.h>


#include "WasmVM.h"
#include "WasmVM_Config.h"



WasmVM* global_WasmVM;


#include <nlohmann/json.hpp>





#if 0

// Basic Panel Phase Drawing Callback
int PanelDrawCallback(XPLMDrawingPhase inPhase, int inIsBefore, void* inRefcon) {

    //std::cout << "waxi/ panel draw callback fired..\n";

    if (inPhase == xplm_Phase_Window) {
        glPushAttrib(GL_ALL_ATTRIB_BITS);

        // Set drawing color to green
        glColor3f(1.0f, 0.0f, 1.0f);

        // Draw a simple rectangle
        glBegin(GL_LINE_LOOP);
            glVertex2f(50.0f, 50.0f);
            glVertex2f(250.0f, 50.0f);
            glVertex2f(250.0f, 150.0f);
            glVertex2f(50.0f, 150.0f);
        glEnd();

        glPopAttrib();
    }
    return 1; // Redraw every frame
}

// Register the panel draw callback
void RegisterPanelDrawCallback() {
    std::cout << "waxi/ proto/ register drawing cb\n";
    XPLMRegisterDrawCallback(PanelDrawCallback, xplm_Phase_Window, 0, nullptr);
}

// Unregister the panel draw callback
void UnregisterPanelDrawCallback() {
    XPLMUnregisterDrawCallback(PanelDrawCallback, xplm_Phase_Window, 0, nullptr);
}

#endif






#if 0
// Flight loop callback function
float CustomFlightLoopCallback(float inElapsedSinceLastCall, float inElapsedTimeSinceLastFlightLoop, int inCounter, void *inRefcon) {
    // Custom logic for the flight loop callback
    //std::cout << "CustomFlightLoopCallback\n";
    //std::cout << "CustomFlightLoopCallback called. Elapsed time: " << inElapsedSinceLastCall << " seconds.\n";

    // Example: Call a function in the WasmVM
    if (global_WasmVM) {
        //global_WasmVM->call_flight_loop(inElapsedSinceLastCall, inElapsedTimeSinceLastFlightLoop, inCounter);
    }

    // Return the interval in seconds for the next callback
    return 0.1f; // Call again in 1 second
}


// Register the flight loop callback
void RegisterFlightLoopCallback() {
    std::cout << "waxi/ registering flcb\n";
    //XPLMRegisterFlightLoopCallback(CustomFlightLoopCallback, 1.0f, nullptr); // Initial interval of 1 second


    // Output the pointer address of CustomFlightLoopCallback as a decimal value
    printf("waxi/ flcb ptr: %p\n", &CustomFlightLoopCallback);

    // Create a flight loop structure
    XPLMCreateFlightLoop_t flightLoopParams;
    flightLoopParams.structSize = sizeof(XPLMCreateFlightLoop_t);
    flightLoopParams.refcon = nullptr;
    flightLoopParams.callbackFunc = CustomFlightLoopCallback;
    flightLoopParams.phase = xplm_FlightLoop_Phase_BeforeFlightModel;
    //flightLoopParams.phase = xplm_FlightLoop_Phase_AfterFlightModel;

    printf("waxi/ handoff: fl_param.callbackFunc: %p\n", flightLoopParams.callbackFunc);

    printf("waxi/ handoff stackptr for flcb_params: %p\n", &flightLoopParams);

    // Create the flight loop
    XPLMFlightLoopID flightLoopID = XPLMCreateFlightLoop(&flightLoopParams);


    // Schedule the flight loop to run
    XPLMScheduleFlightLoop(
        flightLoopID, 
        0.01f, 
        1
    );


}

// Unregister the flight loop callback
void UnregisterFlightLoopCallback() {
    //XPLMUnregisterFlightLoopCallback(CustomFlightLoopCallback, nullptr);
}
#endif


void resolve_paths( WasmVM_Config &config ){

    // Need to determine:
    // - {plugin_root} - done    
    {
        char path[1024]{};
        XPLMGetPluginInfo(XPLMGetMyID(), nullptr, path, nullptr, nullptr);
        //std::cout << "waxi/ plugin_path: ["<< path <<"]\n";

        // Extract the folder path from the full path
        std::string fullPath(path);
        std::string pluginFolderName = fullPath.substr(0, fullPath.find_last_of("/\\")); //strip the plugin filename
        pluginFolderName = pluginFolderName.substr(0, pluginFolderName.find_last_of("/\\")); //strip the os_64 leaf

        config.plugin_folder = pluginFolderName;
        //std::cout << "waxi/ plugin_folder: [" << config.plugin_folder << "]\n";
    }

    // - {xp_root}
    {
        char xpFolder[1024]{};
        XPLMGetSystemPath(xpFolder);
        std::string xpRootPath(xpFolder);
        
        config.xp_folder = xpRootPath;
        //std::cout << "waxi/ xp_folder: [" << config.xp_folder << "]\n";
    }
    
    // - {acf_root}
    {
        char acfFilename[1024]{};
        char acfFolder[1024]{};
        XPLMGetNthAircraftModel(0, acfFilename, acfFolder); // Get the path of the user's aircraft
        //std::cout << "waxi/ Aircraft full path: [" << acfFilename << "]\n";
        //std::cout << "waxi/ Aircraft API folder path: [" << acfFolder << "]\n";
        
        config.acf_folder = acfFolder;
        //std::cout << "waxi/ acf_folder: [" << config.acf_folder << "]\n";
    }

}






// Plugin start function
PLUGIN_API int XPluginStart(char *outName, char *outSig, char *outDesc) {

    XPLMDebugString("WAXI_xpl starting..\n");


    // XPLMEnableFeature("XPLM_WANTS_REFLECTIONS", 1);
    XPLMEnableFeature("XPLM_USE_NATIVE_PATHS", 1);
    //XPLMEnableFeature("XPLM_USE_NATIVE_WIDGET_WINDOWS", 1);
    //XPLMEnableFeature("XPLM_WANTS_DATAREF_NOTIFICATIONS", 1);

    
    WasmVM_Config config;
    // find the aircraft, x-plane and plugin folders.
    resolve_paths( config );


    std::string wasm_filename;

    std::string config_json_filename = config.plugin_folder + "/config.json";
    std::cout << "waxi/ config.json: [" + config_json_filename + "]\n";

    // Load configuration from config.json
    std::ifstream configFile(config_json_filename);
    if (configFile.is_open()) {
        try {
            nlohmann::json jsonConfig;
            configFile >> jsonConfig;


            std::string name, sig, desc;
            if (jsonConfig.contains("name")) {
                name = jsonConfig["name"].get<std::string>();
            }
            if (jsonConfig.contains("sig")) {
                sig = jsonConfig["sig"].get<std::string>();
            }
            if (jsonConfig.contains("desc")) {
                desc = jsonConfig["desc"].get<std::string>();
            }
            std::cout << "waxi/ config.json: name:[" << name << "]  sig:[" << sig << "]  desc:[" << desc << "]\n";
            

            if (jsonConfig.contains("wasm_filename")) {
                wasm_filename = jsonConfig["wasm_filename"].get<std::string>();
                
            }


            //std::cout << "waxi/ Loaded configuration from config.json\n";
        } catch (const std::exception &e) {
            std::cerr << "waxi/ Error parsing config.json: " << e.what() << "\n";
            return 0;
        }
    } else {
        std::cerr << "waxi/ Could not open config.json\n";
        return 0;
    }



    //std::cout << "waxi/ WASM filename:[" << wasm_filename << "]\n";
    global_WasmVM = new WasmVM( wasm_filename, config );


    // These values should be over-written by the call to plugin_start()
    strncpy(outName, "WAXI Default", 255);
    strncpy(outSig, "x-plugins.com/WAXI", 255);
    strncpy(outDesc, "WebAssembly X-Plane Interface", 255);

    // The outName, outSig, and outDesc buffers are guaranteed to be at least 256 bytes each.
    // Reference: X-Plane SDK documentation.

    //XPLMDebugString("waxi/ Calling into wasm start...\n");
    int wasm_ret = global_WasmVM->call_plugin_start( outName, outSig, outDesc );
    
    //XPLMDebugString("waxi/ Plugin started...\n");

    return wasm_ret;
}


// Plugin stop function
PLUGIN_API void XPluginStop(void) {
    // Cleanup code here

    global_WasmVM->call_plugin_stop();

    delete global_WasmVM;
}


// Plugin enable function
PLUGIN_API int XPluginEnable(void) {

    //RegisterPanelDrawCallback();

    // Code to enable the plugin
    return global_WasmVM->call_plugin_enable();;
}


// Plugin disable function
PLUGIN_API void XPluginDisable(void) {
    // Code to disable the plugin
    global_WasmVM->call_plugin_disable();
}


// Plugin receive message function
PLUGIN_API void XPluginReceiveMessage(XPLMPluginID inFromWho, int inMessage, void *inParam) {
    // Handle messages here
    global_WasmVM->call_plugin_message(inFromWho, inMessage, reinterpret_cast<intptr_t>(inParam));
}


