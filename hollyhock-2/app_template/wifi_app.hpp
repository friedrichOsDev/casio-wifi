#pragma once

#include <sdk/os/gui.hpp>
#include "wifi.h" 

// Event IDs
enum {
    ID_WIFI_LIST = 100,
    ID_BTN_CONNECT = 101,
    ID_BTN_CANCEL = 102,
    ID_BTN_PW_OK = 200
};

/**
 * A dialog for entering a WiFi password for a specific SSID.
 */
class WiFiPasswordDialog : public GUIDialog {
    GUILabel m_ssidLabel;
    GUILabel m_passLabel;
    GUITextBox m_passwordInput;
    GUIButton m_okButton;

public:
    WiFiPasswordDialog(const char* ssid) :
        GUIDialog(Height35, AlignCenter, "WiFi Connection", KeyboardStateABC),
        m_ssidLabel(GetLeftX() + 15, GetTopY() + 10, ssid),
        m_passLabel(GetLeftX() + 15, GetTopY() + 30, "Enter Password:"),
        m_passwordInput(GetLeftX() + 15, GetTopY() + 50, GetRightX() - GetLeftX() - 30, "", 64, true),
        m_okButton(GetRightX() - 95, GetTopY() + 90, GetRightX() - 15, GetTopY() + 125, "OK", ID_BTN_PW_OK)
    {
        AddElement(m_ssidLabel);
        AddElement(m_passLabel);
        AddElement(m_passwordInput);
        AddElement(m_okButton);
    }

    virtual int OnEvent(struct GUIDialog_Wrapped *dialog, struct GUIDialog_OnEvent_Data *event) override {
        if (event->GetEventID() == ID_BTN_PW_OK) {
            return DialogResultOK;
        }
        return GUIDialog::OnEvent(dialog, event);
    }

    const char* GetPassword() { return m_passwordInput.GetText(); }
};

/**
 * Main WiFi configuration menu allowing scanning and connecting to networks.
 */
class WiFiMenu : public GUIDialog {
    GUILongLabel m_statusLabel;
    GUIDropDownMenu m_networkList;
    GUIButton m_cancelButton;
    GUIButton m_connectButton;

    wifi_scan_list_t* m_scanList = nullptr;
    GUIDropDownMenuItem* m_menuItems[32];
    int m_numMenuItems = 0;
    int m_selectedIndex = 1;

public:
    WiFiMenu() :
        GUIDialog(Height95, AlignCenter, "WiFi Settings", KeyboardStateNone),
        m_statusLabel(GetLeftX() + 10, GetTopY() + 15, GetRightX() - 10, GetTopY() + 45, "Status: Scanning..."),
        m_networkList(GetLeftX() + 10, GetTopY() + 50, GetRightX() - 10, GetTopY() + 185, ID_WIFI_LIST),
        m_cancelButton(GetLeftX() + 15, GetTopY() + 200, GetLeftX() + 105, GetTopY() + 240, "Exit", ID_BTN_CANCEL),
        m_connectButton(GetRightX() - 105, GetTopY() + 200, GetRightX() - 15, GetTopY() + 240, "Connect", ID_BTN_CONNECT)
    {
        for (int i = 0; i < 32; i++) m_menuItems[i] = nullptr;

        AddElement(m_statusLabel);
        AddElement(m_networkList);
        AddElement(m_cancelButton);
        AddElement(m_connectButton);

        UpdateStatus();
        RefreshNetworks();
    }

    void UpdateStatus() {
        uint8_t status = wifi_status();
        switch (status) {
            case WIFI_STATUS_CONNECTED:    m_statusLabel.SetText("Status: Connected"); break;
            case WIFI_STATUS_DISCONNECTED: m_statusLabel.SetText("Status: Disconnected"); break;
            case WIFI_STATUS_CONNECTING:   m_statusLabel.SetText("Status: Connecting..."); break;
            default:                       m_statusLabel.SetText("Status: Unknown"); break;
        }
        m_statusLabel.Refresh();
        this->Refresh();
    }

    void RefreshNetworks() {
        if (m_scanList) {
            wifi_free_scan_list();
            m_scanList = nullptr;
        }

        for (int i = 0; i < m_numMenuItems; i++) {
            if (m_menuItems[i]) delete m_menuItems[i];
            m_menuItems[i] = nullptr;
        }
        m_numMenuItems = 0;

        m_scanList = wifi_scan();
        if (m_scanList && m_scanList->count > 0) {
            for (int i = 0; i < m_scanList->count && i < 32; i++) {
                m_menuItems[i] = new GUIDropDownMenuItem(m_scanList->items[i].ssid, i + 1, GUIDropDownMenuItem::FlagEnabled  | GUIDropDownMenuItem::FlagTextAlignLeft);
                m_networkList.AddMenuItem(*m_menuItems[i]);
                m_numMenuItems++;
            }
            m_networkList.SetScrollBarVisibility(GUIDropDownMenu::ScrollBarVisibleWhenRequired);
        }
        this->Refresh();
    }

    virtual int OnEvent(struct GUIDialog_Wrapped *dialog, struct GUIDialog_OnEvent_Data *event) override {
        uint16_t eventID = event->GetEventID();
        
        if (eventID == ID_WIFI_LIST && (event->type & 0xF) == 0xD) {
            m_selectedIndex = event->data;
        }

        if (eventID == ID_BTN_CANCEL) return DialogResultCancel;

        if (eventID == ID_BTN_CONNECT) {
            if (m_scanList && m_selectedIndex >= 1 && m_selectedIndex <= m_scanList->count) {
                const char* ssid = m_scanList->items[m_selectedIndex - 1].ssid;
                    
                WiFiPasswordDialog pwDialog(ssid);
                if (pwDialog.ShowDialog() == DialogResultOK) {
                    wifi_connect(ssid, pwDialog.GetPassword());
                    
                    UpdateStatus();
                }
            }
            return 0;
        }

        return GUIDialog::OnEvent(dialog, event);
    }

    ~WiFiMenu() {
        for (int i = 0; i < m_numMenuItems; i++) {
            if (m_menuItems[i]) delete m_menuItems[i];
        }
        if (m_scanList) wifi_free_scan_list();
    }
};
