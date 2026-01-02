/*
 * NVCP Toggle - Native C Implementation
 * Toggles NVIDIA display color settings (vibrance, hue) and Windows gamma ramp
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

/* Include NVAPI */
#include "nvapi/nvapi.h"

/*
 * Undocumented NVAPI function IDs for Digital Vibrance Control and HUE
 * From: https://github.com/falahati/NvAPIWrapper/blob/master/NvAPIWrapper/Native/Helpers/FunctionId.cs
 */
#define NVAPI_GPU_GETDVCINFO        0x4085DE45
#define NVAPI_GPU_SETDVCLEVEL       0x172409B4
#define NVAPI_GPU_GETHUEINFO        0x95B64341
#define NVAPI_GPU_SETHUEANGLE       0xF5A0F22C

/* DVC Info structure (version 1) */
typedef struct {
    NvU32 version;
    NvS32 currentLevel;
    NvS32 minLevel;
    NvS32 maxLevel;
} NV_GPU_DVC_INFO_V1;

#define NV_GPU_DVC_INFO_VER1 MAKE_NVAPI_VERSION(NV_GPU_DVC_INFO_V1, 1)

/* HUE Info structure (version 1) */
typedef struct {
    NvU32 version;
    NvS32 currentAngle;
    NvS32 defaultAngle;
} NV_GPU_HUE_INFO_V1;

#define NV_GPU_HUE_INFO_VER1 MAKE_NVAPI_VERSION(NV_GPU_HUE_INFO_V1, 1)

/* Function pointer types for undocumented APIs */
typedef NvAPI_Status (*PFNNVAPI_GPU_GETDVCINFO)(NvDisplayHandle, NvU32, NV_GPU_DVC_INFO_V1*);
typedef NvAPI_Status (*PFNNVAPI_GPU_SETDVCLEVEL)(NvDisplayHandle, NvU32, NvS32);
typedef NvAPI_Status (*PFNNVAPI_GPU_GETHUEINFO)(NvDisplayHandle, NvU32, NV_GPU_HUE_INFO_V1*);
typedef NvAPI_Status (*PFNNVAPI_GPU_SETHUEANGLE)(NvDisplayHandle, NvU32, NvS32);

/* Global function pointers */
static PFNNVAPI_GPU_GETDVCINFO  pfnNvAPI_GPU_GetDVCInfo = NULL;
static PFNNVAPI_GPU_SETDVCLEVEL pfnNvAPI_GPU_SetDVCLevel = NULL;
static PFNNVAPI_GPU_GETHUEINFO  pfnNvAPI_GPU_GetHUEInfo = NULL;
static PFNNVAPI_GPU_SETHUEANGLE pfnNvAPI_GPU_SetHUEAngle = NULL;

/* Configuration */
typedef struct {
    bool toggleAllDisplays;
    bool keyPressToExit;
    int vibrance;
    int hue;
    double brightness;
    double contrast;
    double gamma;
    int temperature;  /* -100 (cool/blue) to +100 (warm/yellow) */
} Config;

/* Default values (in percentage, 0-100 scale) */
static const int DEFAULT_VIBRANCE_PCT = 50;  /* 50% = neutral/default in NVCP */
static const int DEFAULT_HUE = 0;
static const double DEFAULT_BRIGHTNESS = 0.5;
static const double DEFAULT_CONTRAST = 0.5;
static const double DEFAULT_GAMMA = 1.0;

/* Query interface function - needed to get undocumented functions */
typedef void* (*NvAPI_QueryInterface_t)(unsigned int offset);
static NvAPI_QueryInterface_t NvAPI_QueryInterface = NULL;

/*
 * Initialize undocumented NVAPI functions by querying their addresses
 */
static bool InitUndocumentedNvAPI(void) {
    HMODULE hNvapi = NULL;

#ifdef _WIN64
    hNvapi = LoadLibraryA("nvapi64.dll");
#else
    hNvapi = LoadLibraryA("nvapi.dll");
#endif

    if (!hNvapi) {
        printf("ERROR: Could not load nvapi dll\n");
        return false;
    }

    NvAPI_QueryInterface = (NvAPI_QueryInterface_t)GetProcAddress(hNvapi, "nvapi_QueryInterface");
    if (!NvAPI_QueryInterface) {
        printf("ERROR: Could not find nvapi_QueryInterface\n");
        return false;
    }

    pfnNvAPI_GPU_GetDVCInfo = (PFNNVAPI_GPU_GETDVCINFO)NvAPI_QueryInterface(NVAPI_GPU_GETDVCINFO);
    pfnNvAPI_GPU_SetDVCLevel = (PFNNVAPI_GPU_SETDVCLEVEL)NvAPI_QueryInterface(NVAPI_GPU_SETDVCLEVEL);
    pfnNvAPI_GPU_GetHUEInfo = (PFNNVAPI_GPU_GETHUEINFO)NvAPI_QueryInterface(NVAPI_GPU_GETHUEINFO);
    pfnNvAPI_GPU_SetHUEAngle = (PFNNVAPI_GPU_SETHUEANGLE)NvAPI_QueryInterface(NVAPI_GPU_SETHUEANGLE);

    return true;
}

/*
 * Build gamma ramp from brightness, contrast, gamma, and temperature values
 * Temperature: -100 (cool/blue) to +100 (warm/yellow)
 */
static void BuildGammaRamp(WORD ramp[3][256], double brightness, double contrast, double gamma, int temperature) {
    /* Temperature adjustments: warm boosts red, reduces blue; cool does opposite */
    double tempFactor = temperature / 100.0;  /* -1.0 to +1.0 */
    double redAdj = 1.0 + (tempFactor * 0.1);    /* Warm: +10% red max */
    double blueAdj = 1.0 - (tempFactor * 0.1);   /* Warm: -10% blue max */
    double greenAdj = 1.0 + (tempFactor * 0.02); /* Slight green shift for natural warmth */

    for (int i = 0; i < 256; i++) {
        /* Normalize to 0-1 */
        double value = (double)i / 255.0;

        /* Apply gamma correction */
        if (gamma != 1.0) {
            value = pow(value, 1.0 / gamma);
        }

        /* Apply brightness and contrast */
        /* brightness: 0.5 = normal, contrast: 0.5 = normal */
        value = (value - 0.5) * (contrast * 2.0) + 0.5 + (brightness - 0.5);

        /* Clamp base value to [0, 1] */
        if (value < 0.0) value = 0.0;
        if (value > 1.0) value = 1.0;

        /* Apply temperature per channel */
        double r = value * redAdj;
        double g = value * greenAdj;
        double b = value * blueAdj;

        /* Clamp each channel */
        if (r > 1.0) r = 1.0;
        if (g > 1.0) g = 1.0;
        if (b > 1.0) b = 1.0;

        /* Scale to 16-bit with proper rounding */
        ramp[0][i] = (WORD)(r * 65535.0 + 0.5); /* Red */
        ramp[1][i] = (WORD)(g * 65535.0 + 0.5); /* Green */
        ramp[2][i] = (WORD)(b * 65535.0 + 0.5); /* Blue */
    }
}

/*
 * Check if current gamma ramp matches default (linear)
 */
static bool HasDefaultGammaRamp(HDC hdc) {
    WORD currentRamp[3][256];
    WORD defaultRamp[3][256];

    if (!GetDeviceGammaRamp(hdc, currentRamp)) {
        return true; /* Assume default if we can't read */
    }

    BuildGammaRamp(defaultRamp, DEFAULT_BRIGHTNESS, DEFAULT_CONTRAST, DEFAULT_GAMMA, 0);

    for (int c = 0; c < 3; c++) {
        for (int i = 0; i < 256; i++) {
            /* Allow small tolerance for floating point differences */
            if (abs((int)currentRamp[c][i] - (int)defaultRamp[c][i]) > 256) {
                return false;
            }
        }
    }

    return true;
}

/*
 * Get current digital vibrance level
 */
static int GetVibrance(NvDisplayHandle hDisplay, int* outMin, int* outMax) {
    if (!pfnNvAPI_GPU_GetDVCInfo) return 0;  /* 0 = 50% in NVCP (default) */

    NV_GPU_DVC_INFO_V1 dvcInfo = {0};
    dvcInfo.version = NV_GPU_DVC_INFO_VER1;

    NvAPI_Status status = pfnNvAPI_GPU_GetDVCInfo(hDisplay, 0, &dvcInfo);
    if (status != NVAPI_OK) {
        return 0;  /* 0 = 50% in NVCP (default) */
    }

    if (outMin) *outMin = dvcInfo.minLevel;
    if (outMax) *outMax = dvcInfo.maxLevel;

    return dvcInfo.currentLevel;
}

/*
 * Set digital vibrance level
 */
static bool SetVibrance(NvDisplayHandle hDisplay, int level) {
    if (!pfnNvAPI_GPU_SetDVCLevel) return false;

    NvAPI_Status status = pfnNvAPI_GPU_SetDVCLevel(hDisplay, 0, level);
    return status == NVAPI_OK;
}

/*
 * Get current HUE angle
 */
static int GetHue(NvDisplayHandle hDisplay) {
    if (!pfnNvAPI_GPU_GetHUEInfo) return DEFAULT_HUE;

    NV_GPU_HUE_INFO_V1 hueInfo = {0};
    hueInfo.version = NV_GPU_HUE_INFO_VER1;

    NvAPI_Status status = pfnNvAPI_GPU_GetHUEInfo(hDisplay, 0, &hueInfo);
    if (status != NVAPI_OK) {
        return DEFAULT_HUE;
    }

    return hueInfo.currentAngle;
}

/*
 * Set HUE angle
 */
static bool SetHue(NvDisplayHandle hDisplay, int angle) {
    if (!pfnNvAPI_GPU_SetHUEAngle) return false;

    NvAPI_Status status = pfnNvAPI_GPU_SetHUEAngle(hDisplay, 0, angle);
    return status == NVAPI_OK;
}

/*
 * Parse a simple config file (key=value format)
 */
static bool LoadConfig(const char* filename, Config* config) {
    FILE* f = fopen(filename, "r");
    if (!f) {
        printf("ERROR: Could not open config file: %s\n", filename);
        return false;
    }

    /* Set defaults */
    config->toggleAllDisplays = false;
    config->keyPressToExit = true;
    config->vibrance = 80;
    config->hue = 7;
    config->brightness = 0.60;
    config->contrast = 0.65;
    config->gamma = 1.43;
    config->temperature = 0;

    char line[256];
    while (fgets(line, sizeof(line), f)) {
        /* Skip comments and empty lines */
        if (line[0] == '#' || line[0] == '\n' || line[0] == '\r') continue;

        char key[64], value[64];
        if (sscanf(line, "%63[^=]=%63s", key, value) == 2) {
            /* Trim whitespace */
            char* k = key;
            while (*k == ' ' || *k == '\t') k++;
            char* v = value;
            while (*v == ' ' || *v == '\t') v++;

            if (strcmp(k, "toggleAllDisplays") == 0) {
                config->toggleAllDisplays = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
            } else if (strcmp(k, "keyPressToExit") == 0) {
                config->keyPressToExit = (strcmp(v, "true") == 0 || strcmp(v, "1") == 0);
            } else if (strcmp(k, "vibrance") == 0) {
                config->vibrance = atoi(v);
            } else if (strcmp(k, "hue") == 0) {
                config->hue = atoi(v);
            } else if (strcmp(k, "brightness") == 0) {
                config->brightness = atof(v);
            } else if (strcmp(k, "contrast") == 0) {
                config->contrast = atof(v);
            } else if (strcmp(k, "gamma") == 0) {
                config->gamma = atof(v);
            } else if (strcmp(k, "temperature") == 0) {
                config->temperature = atoi(v);
                /* Clamp to valid range */
                if (config->temperature < -100) config->temperature = -100;
                if (config->temperature > 100) config->temperature = 100;
            }
        }
    }

    fclose(f);
    return true;
}

/*
 * Get a proper DC for gamma ramp control
 */
static HDC GetGammaRampDC(void) {
    /* Try to get DC for the primary display device */
    DISPLAY_DEVICEA dd;
    dd.cb = sizeof(dd);

    for (DWORD i = 0; EnumDisplayDevicesA(NULL, i, &dd, 0); i++) {
        if (dd.StateFlags & DISPLAY_DEVICE_PRIMARY_DEVICE) {
            HDC hdc = CreateDCA("DISPLAY", dd.DeviceName, NULL, NULL);
            if (hdc) return hdc;
        }
    }

    /* Fallback to screen DC */
    return CreateDCA("DISPLAY", NULL, NULL, NULL);
}

/*
 * Convert NVCP percentage (50-100) to NVAPI DVC raw value (0-max)
 * NVAPI range 0-63 maps to NVCP 50%-100%
 * Formula: raw = (nvcp_pct - 50) * max / 50
 */
static int PercentToDVC(int percent, int dvcMax) {
    if (percent <= 50) return 0;
    if (percent >= 100) return dvcMax;
    return ((percent - 50) * dvcMax) / 50;
}

/*
 * Convert NVAPI DVC raw value (0-max) to NVCP percentage (50-100)
 * Formula: nvcp_pct = 50 + (raw * 50 / max)
 */
static int DVCToPercent(int dvcValue, int dvcMax) {
    if (dvcMax == 0) return 50;
    return 50 + (dvcValue * 50) / dvcMax;
}

/*
 * Toggle display settings for a single display
 */
static void ToggleDisplay(NvDisplayHandle hNvDisplay, HDC hdc, const Config* config, const char* displayName) {
    int dvcMin = 0, dvcMax = 63;  /* Default max if query fails */
    int currentVibranceRaw = GetVibrance(hNvDisplay, &dvcMin, &dvcMax);
    int currentHue = GetHue(hNvDisplay);

    /* Convert current raw DVC to percentage for comparison */
    int defaultVibranceRaw = PercentToDVC(DEFAULT_VIBRANCE_PCT, dvcMax);

    /* Check if at default state (within small tolerance for rounding) */
    bool isDefault = (abs(currentVibranceRaw - defaultVibranceRaw) <= 1 &&
                      currentHue == DEFAULT_HUE &&
                      HasDefaultGammaRamp(hdc));

    printf("Display: %s\n", displayName);

    if (isDefault) {
        /* Toggle ON - apply custom settings */
        int targetVibranceRaw = PercentToDVC(config->vibrance, dvcMax);

        printf("Toggling Custom Settings:\n");
        printf("Vibrance: %d%%  Hue: %d  Temp: %d\n", config->vibrance, config->hue, config->temperature);
        printf("Brightness: %.2f  Contrast: %.2f  Gamma: %.2f\n",
               config->brightness, config->contrast, config->gamma);

        SetVibrance(hNvDisplay, targetVibranceRaw);
        SetHue(hNvDisplay, config->hue);

        WORD ramp[3][256];
        BuildGammaRamp(ramp, config->brightness, config->contrast, config->gamma, config->temperature);
        SetDeviceGammaRamp(hdc, ramp);
    } else {
        /* Toggle OFF - reset to defaults */
        printf("Resetting to default settings...\n");

        SetVibrance(hNvDisplay, defaultVibranceRaw);
        SetHue(hNvDisplay, DEFAULT_HUE);

        WORD ramp[3][256];
        BuildGammaRamp(ramp, DEFAULT_BRIGHTNESS, DEFAULT_CONTRAST, DEFAULT_GAMMA, 0);
        SetDeviceGammaRamp(hdc, ramp);
    }
}

/*
 * Main entry point
 */
int main(int argc, char* argv[]) {
    Config config;
    NvAPI_Status status;

    /* Determine config file path */
    char configPath[MAX_PATH];
    char exePath[MAX_PATH];
    GetModuleFileNameA(NULL, exePath, MAX_PATH);
    char* lastSlash = strrchr(exePath, '\\');
    if (lastSlash) {
        *lastSlash = '\0';
        snprintf(configPath, MAX_PATH, "%s\\native_nvcp_config.ini", exePath);
    } else {
        strcpy(configPath, "native_nvcp_config.ini");
    }

    /* Load configuration */
    if (!LoadConfig(configPath, &config)) {
        printf("Using default configuration values.\n");
    }

    /* Initialize NVAPI */
    status = NvAPI_Initialize();
    if (status != NVAPI_OK) {
        NvAPI_ShortString errorStr;
        NvAPI_GetErrorMessage(status, errorStr);
        printf("ERROR: Unable to initialize NVAPI: %s\n", errorStr);
        if (config.keyPressToExit) {
            printf("\nPress any key to exit...\n");
            getchar();
        }
        return 1;
    }

    /* Initialize undocumented functions for DVC/HUE */
    if (!InitUndocumentedNvAPI()) {
        printf("WARNING: DVC/HUE control may not work\n");
    }

    if (config.toggleAllDisplays) {
        printf("Toggling all displays...\n\n");

        /* Enumerate all NVIDIA displays */
        NvDisplayHandle hDisplay;
        for (int i = 0; NvAPI_EnumNvidiaDisplayHandle(i, &hDisplay) == NVAPI_OK; i++) {
            NvAPI_ShortString displayName;
            if (NvAPI_GetAssociatedNvidiaDisplayName(hDisplay, displayName) != NVAPI_OK) {
                snprintf(displayName, sizeof(displayName), "Display %d", i);
            }

            /* Get DC for this display */
            HDC hdc = CreateDCA("DISPLAY", displayName, NULL, NULL);
            if (!hdc) {
                hdc = GetDC(NULL); /* Fallback to primary */
            }

            ToggleDisplay(hDisplay, hdc, &config, displayName);

            if (hdc != GetDC(NULL)) {
                DeleteDC(hdc);
            }
            printf("\n");
        }
    } else {
        printf("Toggling primary display...\n\n");

        /* Get primary display */
        NvDisplayHandle hDisplay;
        status = NvAPI_EnumNvidiaDisplayHandle(0, &hDisplay);
        if (status != NVAPI_OK) {
            printf("ERROR: No NVIDIA display found\n");
            NvAPI_Unload();
            if (config.keyPressToExit) {
                printf("\nPress any key to exit...\n");
                getchar();
            }
            return 1;
        }

        NvAPI_ShortString displayName;
        if (NvAPI_GetAssociatedNvidiaDisplayName(hDisplay, displayName) != NVAPI_OK) {
            strcpy(displayName, "Primary Display");
        }

        HDC hdc = GetGammaRampDC();
        ToggleDisplay(hDisplay, hdc, &config, displayName);
        DeleteDC(hdc);
    }

    NvAPI_Unload();

    if (config.keyPressToExit) {
        printf("\nPress any key to exit...\n");
        getchar();
    }

    return 0;
}
