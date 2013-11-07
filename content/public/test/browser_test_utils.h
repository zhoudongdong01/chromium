// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
#define CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_

#include <queue>
#include <string>
#include <vector>

#include "base/callback_forward.h"
#include "base/compiler_specific.h"
#include "base/files/scoped_temp_dir.h"
#include "base/memory/ref_counted.h"
#include "base/process/process.h"
#include "base/strings/string16.h"
#include "content/public/browser/notification_observer.h"
#include "content/public/browser/notification_registrar.h"
#include "content/public/browser/web_contents_observer.h"
#include "third_party/WebKit/public/web/WebInputEvent.h"
#include "ui/events/keycodes/keyboard_codes.h"
#include "url/gurl.h"

#if defined(OS_WIN)
#include "base/win/scoped_handle.h"
#endif

class CommandLine;

namespace base {
class RunLoop;
}

namespace gfx {
class Point;
}

// A collections of functions designed for use with content_browsertests and
// browser_tests.
// TO BE CLEAR: any function here must work against both binaries. If it only
// works with browser_tests, it should be in chrome\test\base\ui_test_utils.h.
// If it only works with content_browsertests, it should be in
// content\test\content_browser_test_utils.h.

namespace content {

class BrowserContext;
class MessageLoopRunner;
class RenderViewHost;
class WebContents;

// Generate a URL for a file path including a query string.
GURL GetFileUrlWithQuery(const base::FilePath& path,
                         const std::string& query_string);

// Waits for a load stop for the specified |web_contents|'s controller, if the
// tab is currently web_contents.  Otherwise returns immediately.
void WaitForLoadStop(WebContents* web_contents);

// Causes the specified web_contents to crash. Blocks until it is crashed.
void CrashTab(WebContents* web_contents);

// Simulates clicking at the center of the given tab asynchronously; modifiers
// may contain bits from WebInputEvent::Modifiers.
void SimulateMouseClick(WebContents* web_contents,
                        int modifiers,
                        blink::WebMouseEvent::Button button);

// Simulates clicking at the point |point| of the given tab asynchronously;
// modifiers may contain bits from WebInputEvent::Modifiers.
void SimulateMouseClickAt(WebContents* web_contents,
                          int modifiers,
                          blink::WebMouseEvent::Button button,
                          const gfx::Point& point);

// Simulates asynchronously a mouse enter/move/leave event.
void SimulateMouseEvent(WebContents* web_contents,
                        blink::WebInputEvent::Type type,
                        const gfx::Point& point);

// Sends a key press asynchronously.
// The native code of the key event will be set to InvalidNativeKeycode().
// |key_code| alone is good enough for scenarios that only need the char
// value represented by a key event and not the physical key on the keyboard
// or the keyboard layout.
// For scenarios such as chromoting that need the native code,
// SimulateKeyPressWithCode should be used.
void SimulateKeyPress(WebContents* web_contents,
                      ui::KeyboardCode key_code,
                      bool control,
                      bool shift,
                      bool alt,
                      bool command);

// Sends a key press asynchronously.
// |code| specifies the UIEvents (aka: DOM4Events) value of the key:
// https://dvcs.w3.org/hg/d4e/raw-file/tip/source_respec.htm
// The native code of the key event will be set based on |code|.
// See ui/base/keycodes/vi usb_keycode_map.h for mappings between |code|
// and the native code.
// Examples of the various codes:
//   key_code: VKEY_A
//   code: "KeyA"
//   native key code: 0x001e (for Windows).
//   native key code: 0x0026 (for Linux).
void SimulateKeyPressWithCode(WebContents* web_contents,
                              ui::KeyboardCode key_code,
                              const char* code,
                              bool control,
                              bool shift,
                              bool alt,
                              bool command);

// Allow ExecuteScript* methods to target either a WebContents or a
// RenderViewHost.  Targetting a WebContents means executing script in the
// RenderViewHost returned by WebContents::GetRenderViewHost(), which is the
// "current" RenderViewHost.  Pass a specific RenderViewHost to target, for
// example, a "swapped-out" RenderViewHost.
namespace internal {
class ToRenderViewHost {
 public:
  ToRenderViewHost(WebContents* web_contents);
  ToRenderViewHost(RenderViewHost* render_view_host);

  RenderViewHost* render_view_host() const { return render_view_host_; }

 private:
  RenderViewHost* render_view_host_;
};
}  // namespace internal

// Executes the passed |script| in the frame pointed to by |frame_xpath| (use
// empty string for main frame).  The |script| should not invoke
// domAutomationController.send(); otherwise, your test will hang or be flaky.
// If you want to extract a result, use one of the below functions.
// Returns true on success.
bool ExecuteScriptInFrame(const internal::ToRenderViewHost& adapter,
                          const std::string& frame_xpath,
                          const std::string& script) WARN_UNUSED_RESULT;

// The following methods executes the passed |script| in the frame pointed to by
// |frame_xpath| (use empty string for main frame) and sets |result| to the
// value passed to "window.domAutomationController.send" by the executed script.
// They return true on success, false if the script execution failed or did not
// evaluate to the expected type.
bool ExecuteScriptInFrameAndExtractInt(
    const internal::ToRenderViewHost& adapter,
    const std::string& frame_xpath,
    const std::string& script,
    int* result) WARN_UNUSED_RESULT;
bool ExecuteScriptInFrameAndExtractBool(
    const internal::ToRenderViewHost& adapter,
    const std::string& frame_xpath,
    const std::string& script,
    bool* result) WARN_UNUSED_RESULT;
bool ExecuteScriptInFrameAndExtractString(
    const internal::ToRenderViewHost& adapter,
    const std::string& frame_xpath,
    const std::string& script,
    std::string* result) WARN_UNUSED_RESULT;

// Top-frame script execution helpers (a.k.a., the common case):
bool ExecuteScript(const internal::ToRenderViewHost& adapter,
                   const std::string& script) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractInt(const internal::ToRenderViewHost& adapter,
                                const std::string& script,
                                int* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractBool(const internal::ToRenderViewHost& adapter,
                                 const std::string& script,
                                 bool* result) WARN_UNUSED_RESULT;
bool ExecuteScriptAndExtractString(const internal::ToRenderViewHost& adapter,
                                   const std::string& script,
                                   std::string* result) WARN_UNUSED_RESULT;

// Executes the WebUI resource test runner injecting each resource ID in
// |js_resource_ids| prior to executing the tests.
//
// Returns true if tests ran successfully, false otherwise.
bool ExecuteWebUIResourceTest(const internal::ToRenderViewHost& adapter,
                              const std::vector<int>& js_resource_ids);

// Returns the cookies for the given url.
std::string GetCookies(BrowserContext* browser_context, const GURL& url);

// Sets a cookie for the given url. Returns true on success.
bool SetCookie(BrowserContext* browser_context,
               const GURL& url,
               const std::string& value);

// Watches title changes on a WebContents, blocking until an expected title is
// set.
class TitleWatcher : public WebContentsObserver {
 public:
  // |web_contents| must be non-NULL and needs to stay alive for the
  // entire lifetime of |this|. |expected_title| is the title that |this|
  // will wait for.
  TitleWatcher(WebContents* web_contents,
               const string16& expected_title);
  virtual ~TitleWatcher();

  // Adds another title to watch for.
  void AlsoWaitForTitle(const string16& expected_title);

  // Waits until the title matches either expected_title or one of the titles
  // added with AlsoWaitForTitle. Returns the value of the most recently
  // observed matching title.
  const string16& WaitAndGetTitle() WARN_UNUSED_RESULT;

 private:
  // Overridden WebContentsObserver methods.
  virtual void DidStopLoading(RenderViewHost* render_view_host) OVERRIDE;
  virtual void TitleWasSet(NavigationEntry* entry, bool explicit_set) OVERRIDE;

  void TestTitle();

  std::vector<string16> expected_titles_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  // The most recently observed expected title, if any.
  string16 observed_title_;

  DISALLOW_COPY_AND_ASSIGN(TitleWatcher);
};

// Watches a WebContents and blocks until it is destroyed.
class WebContentsDestroyedWatcher : public WebContentsObserver {
 public:
  explicit WebContentsDestroyedWatcher(WebContents* web_contents);
  virtual ~WebContentsDestroyedWatcher();

  // Waits until the WebContents is destroyed.
  void Wait();

 private:
  // Overridden WebContentsObserver methods.
  virtual void WebContentsDestroyed(WebContents* web_contents) OVERRIDE;

  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(WebContentsDestroyedWatcher);
};

// Watches for responses from the DOMAutomationController and keeps them in a
// queue. Useful for waiting for a message to be received.
class DOMMessageQueue : public NotificationObserver {
 public:
  // Constructs a DOMMessageQueue and begins listening for messages from the
  // DOMAutomationController. Do not construct this until the browser has
  // started.
  DOMMessageQueue();
  virtual ~DOMMessageQueue();

  // Removes all messages in the message queue.
  void ClearQueue();

  // Wait for the next message to arrive. |message| will be set to the next
  // message, if not null. Returns true on success.
  bool WaitForMessage(std::string* message) WARN_UNUSED_RESULT;

  // Overridden NotificationObserver methods.
  virtual void Observe(int type,
                       const NotificationSource& source,
                       const NotificationDetails& details) OVERRIDE;

 private:
  NotificationRegistrar registrar_;
  std::queue<std::string> message_queue_;
  bool waiting_for_message_;
  scoped_refptr<MessageLoopRunner> message_loop_runner_;

  DISALLOW_COPY_AND_ASSIGN(DOMMessageQueue);
};

}  // namespace content

#endif  // CONTENT_PUBLIC_TEST_BROWSER_TEST_UTILS_H_
