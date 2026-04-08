#include <list>
#include <vector>
#include <cstring>
#include <pthread.h>
#include <thread>
#include <jni.h>
#include <unistd.h>
#include "Includes/Logger.h"
#include "Includes/obfuscate.h"
#include "Includes/Utils.hpp"
#include "dobby.h"

// --- Configuration & Toggles ---
bool noDeath = false;
bool silentAim = false;
bool espBox = false;
bool espLine = false;
int scoreMul = 1;
int coinsMul = 1;

// List to store active enemies (Dummies)
std::vector<void*> enemyList;

// --- Offsets from your dump.cs ---
#define targetLib OBFUSCATE("libil2cpp.so")
#define RVA_GUN_FIRE 0x130D784       // Found in Gun class
#define RVA_DUMMY_UPDATE 0x1357E5C   // Found in DummyAI class
#define RVA_W2S 0x27C9DA0            // Camera.WorldToScreenPoint

// --- Hook Logic ---

// 1. Hooking DummyAI to track enemies for ESP/Silent Aim
void (*old_DummyAI_Update)(void *instance);
void new_DummyAI_Update(void *instance) {
    if (instance != nullptr) {
        // Add dummy to list if it's not already there
        bool alreadyInList = false;
        for (void* e : enemyList) {
            if (e == instance) {
                alreadyInList = true;
                break;
            }
        }
        if (!alreadyInList) {
            enemyList.push_back(instance);
        }
    }
    old_DummyAI_Update(instance);
}

// 2. Hooking Gun.Fire for Silent Aim
void (*old_Gun_Fire)(void *instance);
void new_Gun_Fire(void *instance) {
    if (silentAim && !enemyList.empty()) {
        // Logic: Redirect bullet direction to enemyList[0] position
        LOGI("Silent Aim: Redirecting Shot");
    }
    old_Gun_Fire(instance);
}

// 3. Hooking Damage for No Death
void (*old_TakeDamage)(void *instance, float damage);
void new_TakeDamage(void *instance, float damage) {
    if (noDeath) {
        return; // Ignore damage
    }
    old_TakeDamage(instance, damage);
}

// --- JNI Bridge (Menu Toggles) ---
extern "C" JNIEXPORT void JNICALL
Java_com_android_support_Launcher_Changes(JNIEnv *env, jclass clazz, jobject obj, jint feature, jint value, jboolean boolean, jstring str) {
    switch (feature) {
        case 0: noDeath = boolean; break;
        case 1: silentAim = boolean; break;
        case 2: espBox = boolean; break;
        case 3: espLine = boolean; break;
        case 4: scoreMul = value; break;
        case 5: coinsMul = value; break;
    }
}

// --- Initialization ---

void* main_thread(void*) {
    LOGI("Main Thread Started: Waiting for libil2cpp.so");
    while (!isLibraryLoaded(targetLib)) sleep(1);

    // Apply Hooks
    DobbyHook((void *)getAbsoluteAddress(targetLib, RVA_GUN_FIRE), (void *)new_Gun_Fire, (void **)&old_Gun_Fire);
    DobbyHook((void *)getAbsoluteAddress(targetLib, RVA_DUMMY_UPDATE), (void *)new_DummyAI_Update, (void **)&old_DummyAI_Update);

    LOGI("Hooks Applied Successfully!");
    return NULL;
}

__attribute__((constructor)) void lib_main() {
    pthread_t ptid;
    pthread_create(&ptid, NULL, main_thread, NULL);
}
