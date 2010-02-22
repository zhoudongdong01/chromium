// Copyright (c) 2009 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/status/network_menu_button.h"

#include <limits>

#include "app/gfx/canvas.h"
#include "app/gfx/skbitmap_operations.h"
#include "app/l10n_util.h"
#include "app/resource_bundle.h"
#include "base/string_util.h"
#include "chrome/browser/chromeos/status/status_area_host.h"
#include "grit/generated_resources.h"
#include "grit/theme_resources.h"
#include "views/widget/widget.h"
#include "views/window/window.h"

namespace chromeos {

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton

// static
const int NetworkMenuButton::kNumWifiImages = 9;
const int NetworkMenuButton::kThrobDuration = 1000;

NetworkMenuButton::NetworkMenuButton(StatusAreaHost* host)
    : StatusAreaButton(this),
      host_(host),
      ALLOW_THIS_IN_INITIALIZER_LIST(network_menu_(this)),
      ALLOW_THIS_IN_INITIALIZER_LIST(animation_connecting_(this)) {
  animation_connecting_.SetThrobDuration(kThrobDuration);
  animation_connecting_.SetTweenType(SlideAnimation::NONE);
  NetworkChanged(NetworkLibrary::Get());
  NetworkLibrary::Get()->AddObserver(this);
}

NetworkMenuButton::~NetworkMenuButton() {
  NetworkLibrary::Get()->RemoveObserver(this);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, menus::MenuModel implementation:

int NetworkMenuButton::GetItemCount() const {
  return static_cast<int>(menu_items_.size());
}

menus::MenuModel::ItemType NetworkMenuButton::GetTypeAt(int index) const {
  return menu_items_[index].type;
}

string16 NetworkMenuButton::GetLabelAt(int index) const {
  return menu_items_[index].label;
}

const gfx::Font* NetworkMenuButton::GetLabelFontAt(int index) const {
  return (menu_items_[index].flags & FLAG_BOLD) ?
      &ResourceBundle::GetSharedInstance().GetFont(ResourceBundle::BoldFont) :
      NULL;
}

bool NetworkMenuButton::IsItemCheckedAt(int index) const {
  // All menus::MenuModel::TYPE_CHECK menu items are checked.
  return true;
}

bool NetworkMenuButton::GetIconAt(int index, SkBitmap* icon) const {
  if (!menu_items_[index].icon.empty()) {
    // Make icon smaller (if necessary) to look better in the menu.
    static const int kMinSize = 8;
    *icon = SkBitmapOperations::DownsampleByTwoUntilSize(
                menu_items_[index].icon, kMinSize, kMinSize);
    return true;
  }
  return false;
}

bool NetworkMenuButton::IsEnabledAt(int index) const {
  return !(menu_items_[index].flags & FLAG_DISABLED);
}

void NetworkMenuButton::ActivatedAt(int index) {
  // When we are refreshing the menu, ignore menu item activation.
  if (refreshing_menu_)
    return;

  NetworkLibrary* cros = NetworkLibrary::Get();

  if (menu_items_[index].flags & FLAG_OPTIONS) {
    host_->OpenButtonOptions(this);
  } else if (menu_items_[index].flags & FLAG_TOGGLE_ETHERNET) {
    cros->EnableEthernetNetworkDevice(!cros->ethernet_enabled());
  } else if (menu_items_[index].flags & FLAG_TOGGLE_WIFI) {
    cros->EnableWifiNetworkDevice(!cros->wifi_enabled());
  } else if (menu_items_[index].flags & FLAG_TOGGLE_CELLULAR) {
    cros->EnableCellularNetworkDevice(!cros->cellular_enabled());
  } else if (menu_items_[index].flags & FLAG_TOGGLE_OFFLINE) {
    cros->EnableOfflineMode(!cros->offline_mode());
  } else if (menu_items_[index].flags & FLAG_WIFI) {
    activated_wifi_network_ = menu_items_[index].wifi_network;

    // If clicked on a network that we are already connected to or we are
    // currently trying to connect to, then do nothing.
    if (activated_wifi_network_.ssid == cros->wifi_ssid())
      return;

    // If wifi network is not encrypted, then directly connect.
    // Otherwise, we open password dialog window.
    if (!activated_wifi_network_.encrypted) {
      cros->ConnectToWifiNetwork(activated_wifi_network_, string16());
    } else {
      PasswordDialogView* dialog = new PasswordDialogView(this,
         activated_wifi_network_.ssid);
      views::Window* window = views::Window::CreateChromeWindow(
          host_->GetNativeWindow(), gfx::Rect(), dialog);
      // Draw the password dialog right below this button and right aligned.
      gfx::Size size = dialog->GetPreferredSize();
      gfx::Rect rect = bounds();
      gfx::Point point = gfx::Point(rect.width() - size.width(), rect.height());
      ConvertPointToScreen(this, &point);
      window->SetBounds(gfx::Rect(point, size), host_->GetNativeWindow());
      window->Show();
    }
  } else if (menu_items_[index].flags & FLAG_CELLULAR) {
      // If clicked on a network that we are already connected to or we are
      // currently trying to connect to, then do nothing.
      if (menu_items_[index].cellular_network.name == cros->cellular_name())
        return;

      cros->ConnectToCellularNetwork(menu_items_[index].cellular_network);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, PasswordDialogDelegate implementation:

bool NetworkMenuButton::OnPasswordDialogAccept(const std::string& ssid,
                                               const string16& password) {
  NetworkLibrary::Get()->ConnectToWifiNetwork(activated_wifi_network_,
                                              password);
  return true;
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, AnimationDelegate implementation:

void NetworkMenuButton::AnimationProgressed(const Animation* animation) {
  if (animation == &animation_connecting_) {
    // Figure out which image to draw. We want a value between 0-100.
    // 0 reperesents no signal and 100 represents full signal strength.
    int value = static_cast<int>(animation_connecting_.GetCurrentValue()*100.0);
    if (value < 0)
      value = 0;
    else if (value > 100)
      value = 100;
    SetIcon(IconForNetworkStrength(value, false));
    SchedulePaint();
  } else {
    MenuButton::AnimationProgressed(animation);
  }
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, StatusAreaButton implementation:

void NetworkMenuButton::DrawIcon(gfx::Canvas* canvas) {
  // Draw the network icon 4 pixels down to center it.
  // Because the status icon is 24x24 but the images are 24x16.
  static const int kIconVerticalPadding = 4;
  canvas->DrawBitmapInt(icon(), 0, kIconVerticalPadding);

  // Draw badge at (14,14) if there is one.
  static const int x = 14;
  static const int y = 14;
  NetworkLibrary* cros = NetworkLibrary::Get();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (cros->EnsureLoaded()) {
    if (!cros->Connected()) {
      canvas->DrawBitmapInt(
          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED), x, y);
    } else if (cros->cellular_connecting() || cros->cellular_connected()) {
      // TODO(chocobo): Check cellular network 3g/edge.
      canvas->DrawBitmapInt(
          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_3G), x, y);
//      canvas->DrawBitmapInt(
//          *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_EDGE), x, y);
    }
  } else {
    canvas->DrawBitmapInt(
        *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_WARNING), x, y);
  }
}

// Override the DrawIcon method to draw the wifi icon.
// The wifi icon is composed of 1 or more alpha-blended icons to show the
// network strength. We also draw an animation for when there's upload/download
// traffic.
/* TODO(chocobo): Add this code back in when UI is finalized.
void NetworkMenuButton::DrawIcon(gfx::Canvas* canvas) {

  // First draw the base icon.
  canvas->DrawBitmapInt(icon(), 0, 0);

  // If wifi, we draw the wifi signal bars.
  NetworkLibrary* cros = NetworkLibrary::Get();
  if (cros->wifi_connecting() ||
      (!cros->ethernet_connected() && cros->wifi_connected())) {
    ResourceBundle& rb = ResourceBundle::GetSharedInstance();
    // We want a value between 0-1.
    // 0 reperesents no signal and 1 represents full signal strength.
    double value = cros->wifi_connecting() ?
        animation_connecting_.GetCurrentValue() :
        cros->wifi_strength() / 100.0;
    if (value < 0)
      value = 0;
    else if (value > 1)
      value = 1;

    // If we are animating network traffic and not connecting, then we need to
    // figure out if we are to also draw the extra image.
    int downloading_index = -1;
    int uploading_index = -1;
    if (!animation_connecting_.IsAnimating()) {
      // For network animation, we only show animation in one direction.
      // So when we are hiding, we just use 1 minus the value.
      // We have kNumWifiImages + 1 number of states. For the first state, where
      // we are not adding any images, we set the index to -1.
      if (animation_downloading_.IsAnimating()) {
        double value_downloading = animation_downloading_.IsShowing() ?
            animation_downloading_.GetCurrentValue() :
            1.0 - animation_downloading_.GetCurrentValue();
        downloading_index = static_cast<int>(value_downloading *
              nextafter(static_cast<float>(kNumWifiImages + 1), 0)) - 1;
      }
      if (animation_uploading_.IsAnimating()) {
        double value_uploading = animation_uploading_.IsShowing() ?
            animation_uploading_.GetCurrentValue() :
            1.0 - animation_uploading_.GetCurrentValue();
        uploading_index = static_cast<int>(value_uploading *
              nextafter(static_cast<float>(kNumWifiImages + 1), 0)) - 1;
      }
    }

    // We need to determine opacity for each of the kNumWifiImages images.
    // We split the range (0-1) into equal ranges per kNumWifiImages images.
    // For example if kNumWifiImages is 3, then [0-0.33) is the first image and
    // [0.33-0.66) is the second image and [0.66-1] is the last image.
    // For each of the image:
    //   If value < the range of this image, draw at kMinOpacity opacity.
    //   If value > the range of this image, draw at kMaxOpacity-1 opacity.
    //   If value within the range of this image, draw at an opacity value
    //     between kMinOpacity and kMaxOpacity-1 relative to where in the range
    //     value is at.
    double value_per_image = 1.0 / kNumWifiImages;
    SkPaint paint;
    for (int i = 0; i < kNumWifiImages; i++) {
      if (value > value_per_image) {
        paint.setAlpha(kMaxOpacity - 1);
        value -= value_per_image;
      } else {
        // Map value between 0 and value_per_image to [kMinOpacity,kMaxOpacity).
        paint.setAlpha(kMinOpacity + static_cast<int>(value / value_per_image *
            nextafter(static_cast<float>(kMaxOpacity - kMinOpacity), 0)));
        // For following iterations, we want to draw at kMinOpacity.
        // So we set value to 0 here.
        value = 0;
      }
      canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_UP1 + i),
                            0, 0, paint);
      canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_DOWN1 + i),
                            0, 0, paint);

      // Draw network traffic downloading/uploading image if necessary.
      if (i == downloading_index)
        canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_DOWN1P + i),
                              0, 0, paint);
      if (i == uploading_index)
        canvas->DrawBitmapInt(*rb.GetBitmapNamed(IDR_STATUSBAR_WIFI_UP1P + i),
                              0, 0, paint);
    }
  }
}
*/
////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, NetworkLibrary::Observer implementation:

void NetworkMenuButton::NetworkChanged(NetworkLibrary* cros) {
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();
  if (cros->EnsureLoaded()) {
    if (cros->wifi_connecting() || cros->cellular_connecting()) {
      // Start the connecting animation if not running.
      if (!animation_connecting_.IsAnimating()) {
        animation_connecting_.Reset();
        animation_connecting_.StartThrobbing(std::numeric_limits<int>::max());
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS1));
      }
    } else {
      // Stop connecting animation since we are not connecting.
      animation_connecting_.Stop();

      // Always show the higher priority connection first. Ethernet then wifi.
      if (cros->ethernet_connected())
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_WIRED));
      else if (cros->wifi_connected())
        SetIcon(IconForNetworkStrength(cros->wifi_strength(), false));
      else if (cros->cellular_connected())
        SetIcon(IconForNetworkStrength(cros->cellular_strength(), false));
      else
        SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
    }
  } else {
    SetIcon(*rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_BARS0));
  }

  SchedulePaint();
}

void NetworkMenuButton::NetworkTraffic(NetworkLibrary* cros, int traffic_type) {
/* TODO(chocobo): Add this code back in when network traffic UI is finalized.
  if (!cros->ethernet_connected() && cros->wifi_connected() &&
      !cros->wifi_connecting()) {
    // For downloading/uploading animation, we want to force at least one cycle
    // so that it looks smooth. And if we keep downloading/uploading, we will
    // keep calling StartThrobbing which will update the cycle count back to 2.
    if (traffic_type & TRAFFIC_DOWNLOAD)
      animation_downloading_.StartThrobbing(2);
    if (traffic_type & TRAFFIC_UPLOAD)
      animation_uploading_.StartThrobbing(2);
  }
  */
}

// static
SkBitmap NetworkMenuButton::IconForNetworkStrength(int strength, bool black) {
  // Compose wifi icon by superimposing various icons.
  int index = static_cast<int>(strength / 100.0 *
      nextafter(static_cast<float>(kNumWifiImages), 0));
  if (index < 0)
    index = 0;
  if (index >= kNumWifiImages)
    index = kNumWifiImages - 1;
  int base = black ? IDR_STATUSBAR_NETWORK_BARS1_BLACK :
                     IDR_STATUSBAR_NETWORK_BARS1;
  return *ResourceBundle::GetSharedInstance().GetBitmapNamed(base + index);
}

////////////////////////////////////////////////////////////////////////////////
// NetworkMenuButton, views::ViewMenuDelegate implementation:

void NetworkMenuButton::RunMenu(views::View* source, const gfx::Point& pt) {
  refreshing_menu_ = true;
  InitMenuItems();
  network_menu_.Rebuild();
  network_menu_.UpdateStates();
  refreshing_menu_ = false;
  network_menu_.RunMenuAt(pt, views::Menu2::ALIGN_TOPRIGHT);
}

void NetworkMenuButton::InitMenuItems() {
  menu_items_.clear();
  // Populate our MenuItems with the current list of wifi networks.
  NetworkLibrary* cros = NetworkLibrary::Get();
  ResourceBundle& rb = ResourceBundle::GetSharedInstance();

  // Ethernet
  string16 label = l10n_util::GetStringUTF16(
                       IDS_STATUSBAR_NETWORK_DEVICE_ETHERNET);
  SkBitmap icon = cros->ethernet_connecting() || cros->ethernet_connected() ?
      *rb.GetBitmapNamed(IDR_STATUSBAR_WIRED_BLACK) :
      *rb.GetBitmapNamed(IDR_STATUSBAR_NETWORK_DISCONNECTED);
  int flag = (cros->ethernet_connecting() || cros->ethernet_connected()) ?
      FLAG_ETHERNET & FLAG_BOLD : FLAG_ETHERNET;
  menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
      icon, WifiNetwork(), CellularNetwork(), flag));

  // Wifi
  const WifiNetworkVector& wifi_networks = cros->wifi_networks();
  // Wifi networks ssids.
  for (size_t i = 0; i < wifi_networks.size(); ++i) {
    label = ASCIIToUTF16(wifi_networks[i].ssid);
    flag = (wifi_networks[i].ssid == cros->wifi_ssid()) ?
        FLAG_WIFI & FLAG_BOLD : FLAG_WIFI;
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        IconForNetworkStrength(wifi_networks[i].strength, true),
        wifi_networks[i], CellularNetwork(), flag));
  }

  // Cellular
  const CellularNetworkVector& cell_networks = cros->cellular_networks();
  // Cellular networks ssids.
  for (size_t i = 0; i < cell_networks.size(); ++i) {
    label = ASCIIToUTF16(cell_networks[i].name);
    flag = (cell_networks[i].name == cros->cellular_name()) ?
        FLAG_CELLULAR & FLAG_BOLD : FLAG_CELLULAR;
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        IconForNetworkStrength(cell_networks[i].strength, true),
        WifiNetwork(), cell_networks[i], flag));
  }

  // No networks available message.
  if (wifi_networks.empty() && cell_networks.empty()) {
    label = l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_MENU_ITEM_INDENT,
                l10n_util::GetStringUTF16(IDS_STATUSBAR_NO_NETWORKS_MESSAGE));
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        SkBitmap(), WifiNetwork(), CellularNetwork(), FLAG_DISABLED));
  }

  // Separator.
  menu_items_.push_back(MenuItem());

  // TODO(chocobo): Uncomment once we figure out how to do offline mode.
  // Offline mode.
//  menu_items_.push_back(MenuItem(cros->offline_mode() ?
//      menus::MenuModel::TYPE_CHECK : menus::MenuModel::TYPE_COMMAND,
//      l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OFFLINE_MODE),
//      SkBitmap(), WifiNetwork(), CellularNetwork(), FLAG_TOGGLE_OFFLINE));

  // Turn Wifi Off.
  label = cros->wifi_enabled() ?
      l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_DISABLE,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI)) :
      l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ENABLE,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_WIFI));
  menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
      SkBitmap(), WifiNetwork(), CellularNetwork(), FLAG_TOGGLE_WIFI));

  // Turn Cellular Off.
  label = cros->cellular_enabled() ?
      l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_DISABLE,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CELLULAR)) :
      l10n_util::GetStringFUTF16(IDS_STATUSBAR_NETWORK_DEVICE_ENABLE,
          l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_DEVICE_CELLULAR));
  menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
      SkBitmap(), WifiNetwork(), CellularNetwork(), FLAG_TOGGLE_CELLULAR));

  if (host_->ShouldOpenButtonOptions(this)) {
    // Separator.
    menu_items_.push_back(MenuItem());

    // Network settings.
    label =
        l10n_util::GetStringUTF16(IDS_STATUSBAR_NETWORK_OPEN_OPTIONS_DIALOG);
    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND, label,
        SkBitmap(), WifiNetwork(), CellularNetwork(), FLAG_OPTIONS));
  }

  // IP address
  if (cros->Connected()) {
    // Separator.
    menu_items_.push_back(MenuItem());

    menu_items_.push_back(MenuItem(menus::MenuModel::TYPE_COMMAND,
        ASCIIToUTF16(cros->IPAddress()), SkBitmap(),
        WifiNetwork(), CellularNetwork(), FLAG_DISABLED));
  }
}

}  // namespace chromeos
