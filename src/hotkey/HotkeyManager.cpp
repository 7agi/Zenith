#include "HotkeyManager.h"
#include <algorithm>
#include <cassert>

namespace zenith {

HotkeyManager* HotkeyManager::s_instance = nullptr;

// ---------------------------------------------------------------------------
HotkeyManager::HotkeyManager() {
    assert(s_instance == nullptr && "Only one HotkeyManager may exist");
    s_instance = this;
}

HotkeyManager::~HotkeyManager() {
    uninstall();
    s_instance = nullptr;
}

// ---------------------------------------------------------------------------
bool HotkeyManager::install() {
    if (m_hook) return true;  // already installed

    m_hook = SetWindowsHookExW(WH_KEYBOARD_LL, hookProc, GetModuleHandleW(nullptr), 0);
    return m_hook != nullptr;
}

void HotkeyManager::uninstall() {
    if (m_hook) {
        UnhookWindowsHookEx(m_hook);
        m_hook = nullptr;
    }
}

// ---------------------------------------------------------------------------
int HotkeyManager::registerCombo(const std::vector<int>& vkeys,
                                  HotkeyCallback onPress,
                                  HotkeyCallback onRelease) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Registration reg;
    reg.id        = m_nextId++;
    reg.vkeys     = vkeys;
    std::sort(reg.vkeys.begin(), reg.vkeys.end());
    reg.onPress   = std::move(onPress);
    reg.onRelease = std::move(onRelease);
    m_registrations.push_back(std::move(reg));
    return reg.id;
}

int HotkeyManager::registerToggle(const std::vector<int>& vkeys,
                                   HotkeyCallback onStart,
                                   HotkeyCallback onStop) {
    std::lock_guard<std::mutex> lock(m_mutex);
    Registration reg;
    reg.id          = m_nextId++;
    reg.vkeys       = vkeys;
    std::sort(reg.vkeys.begin(), reg.vkeys.end());
    reg.isToggle    = true;
    reg.toggleState = false;
    reg.onPress     = std::move(onStart);
    reg.onStop      = std::move(onStop);
    m_registrations.push_back(std::move(reg));
    return reg.id;
}

void HotkeyManager::unregister(int id) {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registrations.erase(
        std::remove_if(m_registrations.begin(), m_registrations.end(),
                       [id](const Registration& r) { return r.id == id; }),
        m_registrations.end());
}

void HotkeyManager::unregisterAll() {
    std::lock_guard<std::mutex> lock(m_mutex);
    m_registrations.clear();
}

// ---------------------------------------------------------------------------
void HotkeyManager::onKeyDown(int vk) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Add to held set (avoid duplicates)
    if (std::find(m_heldKeys.begin(), m_heldKeys.end(), vk) == m_heldKeys.end()) {
        m_heldKeys.push_back(vk);
    }

    // Check all registrations
    for (auto& reg : m_registrations) {
        if (reg.fired) continue;  // already active, wait for release

        // All keys in the combo must be currently held
        bool allHeld = true;
        for (int k : reg.vkeys) {
            if (std::find(m_heldKeys.begin(), m_heldKeys.end(), k) == m_heldKeys.end()) {
                allHeld = false;
                break;
            }
        }

        if (allHeld) {
            reg.fired = true;
            if (reg.isToggle) {
                if (!reg.toggleState) {
                    reg.toggleState = true;
                    if (reg.onPress) reg.onPress();
                } else {
                    reg.toggleState = false;
                    if (reg.onStop) reg.onStop();
                }
            } else {
                if (reg.onPress) reg.onPress();
            }
        }
    }
}

void HotkeyManager::onKeyUp(int vk) {
    std::lock_guard<std::mutex> lock(m_mutex);

    // Remove from held set
    m_heldKeys.erase(
        std::remove(m_heldKeys.begin(), m_heldKeys.end(), vk),
        m_heldKeys.end());

    // For non-toggle registrations that had fired: if any of their keys was
    // released, mark as unfired and fire onRelease callback.
    for (auto& reg : m_registrations) {
        if (!reg.fired || reg.isToggle) continue;

        bool anyReleased = (std::find(reg.vkeys.begin(), reg.vkeys.end(), vk)
                            != reg.vkeys.end());
        if (anyReleased) {
            reg.fired = false;
            if (reg.onRelease) reg.onRelease();
        }
    }
}

// ---------------------------------------------------------------------------
// Static hook proc
// ---------------------------------------------------------------------------
LRESULT CALLBACK HotkeyManager::hookProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (nCode == HC_ACTION && s_instance) {
        auto* kbs = reinterpret_cast<KBDLLHOOKSTRUCT*>(lParam);
        int vk = static_cast<int>(kbs->vkCode);

        if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN) {
            s_instance->onKeyDown(vk);
        } else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
            s_instance->onKeyUp(vk);
        }
    }
    return CallNextHookEx(nullptr, nCode, wParam, lParam);
}

} // namespace zenith
