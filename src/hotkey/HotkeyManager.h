#pragma once
#include <windows.h>
#include <vector>
#include <functional>
#include <unordered_map>
#include <string>
#include <mutex>

namespace zenith {

// ---------------------------------------------------------------------------
// A combo = ordered set of virtual key codes that must all be held.
// ---------------------------------------------------------------------------
struct HotkeyCombo;

// Callback type: fired when the combo is matched.
using HotkeyCallback = std::function<void()>;

// ---------------------------------------------------------------------------
// HotkeyManager
//
// Uses a low-level keyboard hook (WH_KEYBOARD_LL) installed on the calling
// thread.  Call `processMessages()` in your message loop or run via a
// dedicated thread.
//
// Features:
//  - Multi-key combos: all keys in the combo must be held simultaneously.
//  - Toggle detection: if a combo's start and stop callbacks point to the
//    same registered ID, the manager internally tracks state and alternates.
//  - No false positives from partial holds (fixes the Ascent bug).
// ---------------------------------------------------------------------------
class HotkeyManager {
public:
    HotkeyManager();
    ~HotkeyManager();

    // Not copyable
    HotkeyManager(const HotkeyManager&) = delete;
    HotkeyManager& operator=(const HotkeyManager&) = delete;

    // -----------------------------------------------------------------------
    // Register a combo.  Returns an ID you can use to unregister it.
    // `onPress`  is called when ALL keys in the combo are newly held.
    // `onRelease` (optional) is called when ANY key in the combo is released
    //              AFTER a press event has fired.
    // -----------------------------------------------------------------------
    int registerCombo(const std::vector<int>& vkeys,
                      HotkeyCallback onPress,
                      HotkeyCallback onRelease = nullptr);

    // Register a TOGGLE combo: same hotkey starts/stops.
    // `onStart` fires on odd presses, `onStop` fires on even presses.
    int registerToggle(const std::vector<int>& vkeys,
                       HotkeyCallback onStart,
                       HotkeyCallback onStop);

    void unregister(int id);
    void unregisterAll();

    // Call once per message loop iteration (or dedicated thread).
    // Installs the hook if not yet installed.
    bool install();   // call from the thread that will run the message loop
    void uninstall();

    // -----------------------------------------------------------------------
    // Internal – called by the static hook proc.
    // -----------------------------------------------------------------------
    void onKeyDown(int vk);
    void onKeyUp(int vk);

    // Singleton-style access (hook proc needs it)
    static HotkeyManager* instance() { return s_instance; }

private:
    struct Registration {
        int                 id;
        std::vector<int>    vkeys;      // sorted for easy comparison
        HotkeyCallback      onPress;
        HotkeyCallback      onRelease;
        bool                isToggle     = false;
        bool                toggleState  = false;  // false=start, true=stop
        HotkeyCallback      onStop;
        bool                fired        = false;   // press event fired, awaiting release
    };

    void checkCombos();

    static LRESULT CALLBACK hookProc(int nCode, WPARAM wParam, LPARAM lParam);

    HHOOK                        m_hook    = nullptr;
    int                          m_nextId  = 1;
    std::vector<int>             m_heldKeys;     // currently held vkeys
    std::vector<Registration>    m_registrations;
    mutable std::mutex           m_mutex;

    static HotkeyManager*        s_instance;
};

} // namespace zenith
