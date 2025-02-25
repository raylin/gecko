/* -*- Mode: C++; tab-width: 8; indent-tabs-mode: nil; c-basic-offset: 2 -*- */
/* vim: set ts=8 sts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

#ifndef nsGlobalWindowInner_h___
#define nsGlobalWindowInner_h___

#include "nsPIDOMWindow.h"

#include "nsTHashtable.h"
#include "nsHashKeys.h"
#include "nsRefPtrHashtable.h"
#include "nsInterfaceHashtable.h"

// Local Includes
// Helper Classes
#include "nsCOMPtr.h"
#include "nsAutoPtr.h"
#include "nsWeakReference.h"
#include "nsDataHashtable.h"
#include "nsJSThingHashtable.h"
#include "nsCycleCollectionParticipant.h"

// Interfaces Needed
#include "nsIBrowserDOMWindow.h"
#include "nsIDOMEventTarget.h"
#include "nsIInterfaceRequestor.h"
#include "nsIDOMChromeWindow.h"
#include "nsIScriptGlobalObject.h"
#include "nsIScriptObjectPrincipal.h"
#include "nsITimer.h"
#include "nsIDOMModalContentWindow.h"
#include "mozilla/EventListenerManager.h"
#include "nsIPrincipal.h"
#include "nsSize.h"
#include "mozilla/FlushType.h"
#include "prclist.h"
#include "mozilla/dom/BindingDeclarations.h"
#include "mozilla/dom/StorageEvent.h"
#include "mozilla/dom/StorageEventBinding.h"
#include "mozilla/dom/UnionTypes.h"
#include "mozilla/ErrorResult.h"
#include "nsFrameMessageManager.h"
#include "mozilla/Attributes.h"
#include "mozilla/GuardObjects.h"
#include "mozilla/LinkedList.h"
#include "mozilla/TimeStamp.h"
#include "nsWrapperCacheInlines.h"
#include "nsIIdleObserver.h"
#include "nsIDocument.h"
#include "mozilla/dom/EventTarget.h"
#include "mozilla/dom/WindowBinding.h"
#include "Units.h"
#include "nsComponentManagerUtils.h"
#include "nsSize.h"
#include "nsCheapSets.h"
#include "mozilla/dom/ImageBitmapSource.h"
#include "mozilla/UniquePtr.h"

class nsIArray;
class nsIBaseWindow;
class nsIContent;
class nsICSSDeclaration;
class nsIDocShellTreeOwner;
class nsIDOMOfflineResourceList;
class nsIScrollableFrame;
class nsIControllers;
class nsIJSID;
class nsIScriptContext;
class nsIScriptTimeoutHandler;
class nsITabChild;
class nsITimeoutHandler;
class nsIWebBrowserChrome;
class mozIDOMWindowProxy;

class nsDOMWindowList;
class nsScreen;
class nsHistory;
class nsGlobalWindowObserver;
class nsGlobalWindowOuter;
class nsDOMWindowUtils;
class nsIIdleService;
struct nsRect;

class nsWindowSizes;

class IdleRequestExecutor;

class DialogValueHolder;

namespace mozilla {
class AbstractThread;
class DOMEventTargetHelper;
class ThrottledEventQueue;
namespace dom {
class BarProp;
struct ChannelPixelLayout;
class Console;
class Crypto;
class CustomElementRegistry;
class DocGroup;
class External;
class Function;
class Gamepad;
enum class ImageBitmapFormat : uint8_t;
class IdleRequest;
class IdleRequestCallback;
class IncrementalRunnable;
class IntlUtils;
class Location;
class MediaQueryList;
class MozSelfSupport;
class Navigator;
class OwningExternalOrWindowProxy;
class Promise;
class PostMessageEvent;
struct RequestInit;
class RequestOrUSVString;
class Selection;
class SpeechSynthesis;
class TabGroup;
class Timeout;
class U2F;
class VRDisplay;
enum class VRDisplayEventReason : uint8_t;
class VREventObserver;
class WakeLock;
#if defined(MOZ_WIDGET_ANDROID)
class WindowOrientationObserver;
#endif
class Worklet;
namespace cache {
class CacheStorage;
} // namespace cache
class IDBFactory;
} // namespace dom
} // namespace mozilla

extern already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx, nsGlobalWindowInner *aWindow,
                          mozilla::dom::Function& aFunction,
                          const mozilla::dom::Sequence<JS::Value>& aArguments,
                          mozilla::ErrorResult& aError);

extern already_AddRefed<nsIScriptTimeoutHandler>
NS_CreateJSTimeoutHandler(JSContext* aCx, nsGlobalWindowInner *aWindow,
                          const nsAString& aExpression,
                          mozilla::ErrorResult& aError);

extern const js::Class OuterWindowProxyClass;

struct IdleObserverHolder
{
  nsCOMPtr<nsIIdleObserver> mIdleObserver;
  uint32_t mTimeInS;
  bool mPrevNotificationIdle;

  IdleObserverHolder()
    : mTimeInS(0), mPrevNotificationIdle(false)
  {
    MOZ_COUNT_CTOR(IdleObserverHolder);
  }

  IdleObserverHolder(const IdleObserverHolder& aOther)
    : mIdleObserver(aOther.mIdleObserver), mTimeInS(aOther.mTimeInS),
      mPrevNotificationIdle(aOther.mPrevNotificationIdle)
  {
    MOZ_COUNT_CTOR(IdleObserverHolder);
  }

  bool operator==(const IdleObserverHolder& aOther) const {
    return
      mIdleObserver == aOther.mIdleObserver &&
      mTimeInS == aOther.mTimeInS;
  }

  ~IdleObserverHolder()
  {
    MOZ_COUNT_DTOR(IdleObserverHolder);
  }
};

inline void
ImplCycleCollectionTraverse(nsCycleCollectionTraversalCallback& aCallback,
                            IdleObserverHolder& aField,
                            const char* aName,
                            unsigned aFlags)
{
  CycleCollectionNoteChild(aCallback, aField.mIdleObserver.get(), aName, aFlags);
}

// NOTE: Currently this file, despite being named mozilla/dom/Window.h, exports
// the class nsGlobalWindowInner. It will be renamed in the future to
// mozilla::dom::Window.

//*****************************************************************************
// nsGlobalWindow: Global Object for Scripting
//*****************************************************************************
// Beware that all scriptable interfaces implemented by
// nsGlobalWindow will be reachable from JS, if you make this class
// implement new interfaces you better know what you're
// doing. Security wise this is very sensitive code. --
// jst@netscape.com

// nsGlobalWindow inherits PRCList for maintaining a list of all inner
// windows still in memory for any given outer window. This list is
// needed to ensure that mOuterWindow doesn't end up dangling. The
// nature of PRCList means that the window itself is always in the
// list, and an outer window's list will also contain all inner window
// objects that are still in memory (and in reality all inner window
// object's lists also contain its outer and all other inner windows
// belonging to the same outer window, but that's an unimportant
// side effect of inheriting PRCList).

class nsGlobalWindowInner : public mozilla::dom::EventTarget,
                            public nsPIDOMWindowInner,
                            private nsIDOMWindow,
                            // NOTE: This interface is private, as it's only
                            // implemented on chrome windows.
                            private nsIDOMChromeWindow,
                            public nsIScriptGlobalObject,
                            public nsIScriptObjectPrincipal,
                            public nsSupportsWeakReference,
                            public nsIInterfaceRequestor,
                            public PRCListStr
{
public:
  typedef mozilla::TimeStamp TimeStamp;
  typedef mozilla::TimeDuration TimeDuration;

  typedef nsDataHashtable<nsUint64HashKey, nsGlobalWindowInner*> InnerWindowByIdTable;

  static void
  AssertIsOnMainThread()
#ifdef DEBUG
  ;
#else
  { }
#endif

  static nsGlobalWindowInner* Cast(nsPIDOMWindowInner* aPIWin) {
    return static_cast<nsGlobalWindowInner*>(aPIWin);
  }
  static const nsGlobalWindowInner* Cast(const nsPIDOMWindowInner* aPIWin) {
    return static_cast<const nsGlobalWindowInner*>(aPIWin);
  }
  static nsGlobalWindowInner* Cast(mozIDOMWindow* aWin) {
    return Cast(nsPIDOMWindowInner::From(aWin));
  }

  static nsGlobalWindowInner*
  GetInnerWindowWithId(uint64_t aInnerWindowID)
  {
    AssertIsOnMainThread();

    if (!sInnerWindowsById) {
      return nullptr;
    }

    nsGlobalWindowInner* innerWindow =
      sInnerWindowsById->Get(aInnerWindowID);
    MOZ_ASSERT(!innerWindow || innerWindow->IsInnerWindow(),
                "Outer window in sInnerWindowsById?");
    return innerWindow;
  }

  static InnerWindowByIdTable* GetWindowsTable() {
    AssertIsOnMainThread();

    return sInnerWindowsById;
  }

  static nsGlobalWindowInner *FromSupports(nsISupports *supports)
  {
    // Make sure this matches the casts we do in QueryInterface().
    return (nsGlobalWindowInner *)(mozilla::dom::EventTarget *)supports;
  }

  static already_AddRefed<nsGlobalWindowInner>
  Create(nsGlobalWindowOuter* aOuter, bool aIsChrome);

  // callback for close event
  void ReallyCloseWindow();

  // nsISupports
  NS_DECL_CYCLE_COLLECTING_ISUPPORTS

  // nsWrapperCache
  virtual JSObject *WrapObject(JSContext *cx, JS::Handle<JSObject*> aGivenProto) override
  {
    return IsInnerWindow() || AsOuter()->EnsureInnerWindow() ? GetWrapper() : nullptr;
  }

  // nsIGlobalJSObjectHolder
  virtual JSObject* GetGlobalJSObject() override;

  // nsIScriptGlobalObject
  JSObject *FastGetGlobalJSObject() const
  {
    return GetWrapperPreserveColor();
  }

  void TraceGlobalJSObject(JSTracer* aTrc);

  virtual nsresult EnsureScriptEnvironment() override;

  virtual nsIScriptContext *GetScriptContext() override;

  virtual bool IsBlackForCC(bool aTracingNeeded = true) override;

  // nsIScriptObjectPrincipal
  virtual nsIPrincipal* GetPrincipal() override;

  // nsIDOMWindow
  NS_DECL_NSIDOMWINDOW

  // nsIDOMChromeWindow (only implemented on chrome windows)
  NS_DECL_NSIDOMCHROMEWINDOW

  void CaptureEvents();
  void ReleaseEvents();
  void Dump(const nsAString& aStr);
  void SetResizable(bool aResizable) const;

  // nsIDOMEventTarget
  NS_DECL_NSIDOMEVENTTARGET

  virtual mozilla::EventListenerManager*
    GetExistingListenerManager() const override;

  virtual mozilla::EventListenerManager*
    GetOrCreateListenerManager() override;

  using mozilla::dom::EventTarget::RemoveEventListener;
  virtual void AddEventListener(const nsAString& aType,
                                mozilla::dom::EventListener* aListener,
                                const mozilla::dom::AddEventListenerOptionsOrBoolean& aOptions,
                                const mozilla::dom::Nullable<bool>& aWantsUntrusted,
                                mozilla::ErrorResult& aRv) override;
  virtual nsPIDOMWindowOuter* GetOwnerGlobalForBindings() override;

  virtual nsIGlobalObject* GetOwnerGlobal() const override;

  // nsPIDOMWindow
  virtual nsPIDOMWindowOuter* GetPrivateRoot() override;

  // Outer windows only.
  virtual void ActivateOrDeactivate(bool aActivate) override;
  virtual void SetActive(bool aActive) override;
  virtual bool IsTopLevelWindowActive() override;
  virtual void SetIsBackground(bool aIsBackground) override;
  virtual void SetChromeEventHandler(mozilla::dom::EventTarget* aChromeEventHandler) override;

  // Outer windows only.
  virtual void SetInitialPrincipalToSubject() override;

  virtual PopupControlState PushPopupControlState(PopupControlState state, bool aForce) const override;
  virtual void PopPopupControlState(PopupControlState state) const override;
  virtual PopupControlState GetPopupControlState() const override;

  virtual already_AddRefed<nsISupports> SaveWindowState() override;
  virtual nsresult RestoreWindowState(nsISupports *aState) override;

  void Suspend();
  void Resume();
  virtual bool IsSuspended() const override;
  void Freeze();
  void Thaw();
  virtual bool IsFrozen() const override;
  void SyncStateFromParentWindow();

  virtual nsresult FireDelayedDOMEvents() override;

  virtual nsresult SetNewDocument(nsIDocument *aDocument,
                                  nsISupports *aState,
                                  bool aForceReuseInnerWindow) override;

  virtual void SetOpenerWindow(nsPIDOMWindowOuter* aOpener,
                               bool aOriginalOpener) override;

  // Outer windows only.
  virtual void EnsureSizeAndPositionUpToDate() override;

  virtual void EnterModalState() override;
  virtual void LeaveModalState() override;

  // Outer windows only.
  virtual bool CanClose() override;
  virtual void ForceClose() override;

  virtual void MaybeUpdateTouchState() override;

  // Outer windows only.
  virtual bool DispatchCustomEvent(const nsAString& aEventName) override;

  // Inner windows only.
  void RefreshCompartmentPrincipal();

  // For accessing protected field mFullScreen
  friend class FullscreenTransitionTask;

  // Outer windows only.
  virtual nsresult SetFullscreenInternal(
    FullscreenReason aReason, bool aIsFullscreen) override final;
  virtual void FullscreenWillChange(bool aIsFullscreen) override final;
  virtual void FinishFullscreenChange(bool aIsFullscreen) override final;

  // Inner windows only.
  virtual void SetHasGamepadEventListener(bool aHasGamepad = true) override;
  void NotifyVREventListenerAdded();
  bool HasUsedVR() const;
  bool IsVRContentDetected() const;
  bool IsVRContentPresenting() const;

  using EventTarget::EventListenerAdded;
  virtual void EventListenerAdded(nsAtom* aType) override;
  using EventTarget::EventListenerRemoved;
  virtual void EventListenerRemoved(nsAtom* aType) override;

  // nsIInterfaceRequestor
  NS_DECL_NSIINTERFACEREQUESTOR

  // WebIDL interface.
  already_AddRefed<nsPIDOMWindowOuter> IndexedGetter(uint32_t aIndex);

  static bool IsPrivilegedChromeWindow(JSContext* /* unused */, JSObject* aObj);

  static bool IsRequestIdleCallbackEnabled(JSContext* aCx, JSObject* /* unused */);

  static bool IsWindowPrintEnabled(JSContext* /* unused */, JSObject* /* unused */);

  bool DoResolve(JSContext* aCx, JS::Handle<JSObject*> aObj,
                 JS::Handle<jsid> aId,
                 JS::MutableHandle<JS::PropertyDescriptor> aDesc);
  // The return value is whether DoResolve might end up resolving the given id.
  // If in doubt, return true.
  static bool MayResolve(jsid aId);

  void GetOwnPropertyNames(JSContext* aCx, JS::AutoIdVector& aNames,
                           bool aEnumerableOnly, mozilla::ErrorResult& aRv);

  already_AddRefed<nsPIDOMWindowOuter> GetTop() override;
  nsPIDOMWindowOuter* GetScriptableTop() override;
  inline nsGlobalWindowOuter *GetTopInternal();

  inline nsGlobalWindowOuter* GetScriptableTopInternal();

  nsPIDOMWindowOuter* GetChildWindow(const nsAString& aName);

  // These return true if we've reached the state in this top level window
  // where we ask the user if further dialogs should be blocked.
  //
  // DialogsAreBeingAbused must be called on the scriptable top inner window.
  //
  // nsGlobalWindowOuter::ShouldPromptToBlockDialogs is implemented in terms of
  // nsGlobalWindowInner::DialogsAreBeingAbused, and will get the scriptable top inner window
  // automatically.
  // Inner windows only.
  bool DialogsAreBeingAbused();

  // These functions are used for controlling and determining whether dialogs
  // (alert, prompt, confirm) are currently allowed in this window.  If you want
  // to temporarily disable dialogs, please use TemporarilyDisableDialogs, not
  // EnableDialogs/DisableDialogs, because correctly determining whether to
  // re-enable dialogs is actually quite difficult.
  void EnableDialogs();
  void DisableDialogs();

  nsIScriptContext *GetContextInternal();

  nsGlobalWindowOuter *GetOuterWindowInternal() const;

  bool IsCreatingInnerWindow() const
  {
    return mCreatingInnerWindow;
  }

  bool IsChromeWindow() const
  {
    return mIsChrome;
  }

  // GetScrollFrame does not flush.  Callers should do it themselves as needed,
  // depending on which info they actually want off the scrollable frame.
  nsIScrollableFrame *GetScrollFrame();

  nsresult Observe(nsISupports* aSubject, const char* aTopic,
                   const char16_t* aData);

  void ObserveStorageNotification(mozilla::dom::StorageEvent* aEvent,
                                  const char16_t* aStorageType,
                                  bool aPrivateBrowsing);

  static void Init();
  static void ShutDown();
  static bool IsCallerChrome();

  void CleanupCachedXBLHandlers();

  friend class WindowStateHolder;

  NS_DECL_CYCLE_COLLECTION_SKIPPABLE_SCRIPT_HOLDER_CLASS_AMBIGUOUS(nsGlobalWindowInner,
                                                                   nsIDOMEventTarget)

#ifdef DEBUG
  // Call Unlink on this window. This may cause bad things to happen, so use
  // with caution.
  void RiskyUnlink();
#endif

  virtual JSObject*
    GetCachedXBLPrototypeHandler(nsXBLPrototypeHandler* aKey) override;

  virtual void
    CacheXBLPrototypeHandler(nsXBLPrototypeHandler* aKey,
                             JS::Handle<JSObject*> aHandler) override;

  virtual bool TakeFocus(bool aFocus, uint32_t aFocusMethod) override;
  virtual void SetReadyForFocus() override;
  virtual void PageHidden() override;
  virtual nsresult DispatchAsyncHashchange(nsIURI *aOldURI, nsIURI *aNewURI) override;
  virtual nsresult DispatchSyncPopState() override;

  // Inner windows only.
  virtual void EnableDeviceSensor(uint32_t aType) override;
  virtual void DisableDeviceSensor(uint32_t aType) override;

#if defined(MOZ_WIDGET_ANDROID)
  virtual void EnableOrientationChangeListener() override;
  virtual void DisableOrientationChangeListener() override;
#endif

  virtual void EnableTimeChangeNotifications() override;
  virtual void DisableTimeChangeNotifications() override;

  virtual nsresult SetArguments(nsIArray* aArguments) override;

  bool IsClosedOrClosing() {
    return (mIsClosed ||
            mInClose ||
            mHavePendingClose ||
            mCleanedUp);
  }

  bool
  IsCleanedUp() const
  {
    return mCleanedUp;
  }

  virtual void
  FirePopupBlockedEvent(nsIDocument* aDoc,
                        nsIURI* aPopupURI,
                        const nsAString& aPopupWindowName,
                        const nsAString& aPopupWindowFeatures) override;

  virtual uint32_t GetSerial() override {
    return mSerial;
  }

  void AddSizeOfIncludingThis(nsWindowSizes& aWindowSizes) const;

  // Inner windows only.
  void AddEventTargetObject(mozilla::DOMEventTargetHelper* aObject);
  void RemoveEventTargetObject(mozilla::DOMEventTargetHelper* aObject);

  void NotifyIdleObserver(IdleObserverHolder* aIdleObserverHolder,
                          bool aCallOnidle);
  nsresult HandleIdleActiveEvent();
  bool ContainsIdleObserver(nsIIdleObserver* aIdleObserver, uint32_t timeInS);
  void HandleIdleObserverCallback();

  void AllowScriptsToClose()
  {
    mAllowScriptsToClose = true;
  }

  enum SlowScriptResponse {
    ContinueSlowScript = 0,
    ContinueSlowScriptAndKeepNotifying,
    AlwaysContinueSlowScript,
    KillSlowScript,
    KillScriptGlobal
  };
  SlowScriptResponse ShowSlowScriptDialog(const nsString& aAddonId);

  // Inner windows only.
  void AddGamepad(uint32_t aIndex, mozilla::dom::Gamepad* aGamepad);
  void RemoveGamepad(uint32_t aIndex);
  void GetGamepads(nsTArray<RefPtr<mozilla::dom::Gamepad> >& aGamepads);
  already_AddRefed<mozilla::dom::Gamepad> GetGamepad(uint32_t aIndex);
  void SetHasSeenGamepadInput(bool aHasSeen);
  bool HasSeenGamepadInput();
  void SyncGamepadState();
  void StopGamepadHaptics();

  // Inner windows only.
  // Enable/disable updates for gamepad input.
  void EnableGamepadUpdates();
  void DisableGamepadUpdates();

  // Inner windows only.
  // Enable/disable updates for VR
  void EnableVRUpdates();
  void DisableVRUpdates();
  // Reset telemetry data when switching windows.
  // aUpdate, true for accumulating the result to the histogram.
  // false for only resetting the timestamp.
  void ResetVRTelemetry(bool aUpdate);

  // Update the VR displays for this window
  bool UpdateVRDisplays(nsTArray<RefPtr<mozilla::dom::VRDisplay>>& aDisplays);

  // Inner windows only.
  // Called to inform that the set of active VR displays has changed.
  void NotifyActiveVRDisplaysChanged();

  void DispatchVRDisplayActivate(uint32_t aDisplayID,
                                 mozilla::dom::VRDisplayEventReason aReason);
  void DispatchVRDisplayDeactivate(uint32_t aDisplayID,
                                   mozilla::dom::VRDisplayEventReason aReason);
  void DispatchVRDisplayConnect(uint32_t aDisplayID);
  void DispatchVRDisplayDisconnect(uint32_t aDisplayID);
  void DispatchVRDisplayPresentChange(uint32_t aDisplayID);

#define EVENT(name_, id_, type_, struct_)                                     \
  mozilla::dom::EventHandlerNonNull* GetOn##name_()                           \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetExistingListenerManager();        \
    return elm ? elm->GetEventHandler(nsGkAtoms::on##name_, EmptyString())    \
               : nullptr;                                                     \
  }                                                                           \
  void SetOn##name_(mozilla::dom::EventHandlerNonNull* handler)               \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager();        \
    if (elm) {                                                                \
      elm->SetEventHandler(nsGkAtoms::on##name_, EmptyString(), handler);     \
    }                                                                         \
  }
#define ERROR_EVENT(name_, id_, type_, struct_)                               \
  mozilla::dom::OnErrorEventHandlerNonNull* GetOn##name_()                    \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetExistingListenerManager();        \
    return elm ? elm->GetOnErrorEventHandler() : nullptr;                     \
  }                                                                           \
  void SetOn##name_(mozilla::dom::OnErrorEventHandlerNonNull* handler)        \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager();        \
    if (elm) {                                                                \
      elm->SetEventHandler(handler);                                          \
    }                                                                         \
  }
#define BEFOREUNLOAD_EVENT(name_, id_, type_, struct_)                        \
  mozilla::dom::OnBeforeUnloadEventHandlerNonNull* GetOn##name_()             \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetExistingListenerManager();        \
    return elm ? elm->GetOnBeforeUnloadEventHandler() : nullptr;              \
  }                                                                           \
  void SetOn##name_(mozilla::dom::OnBeforeUnloadEventHandlerNonNull* handler) \
  {                                                                           \
    mozilla::EventListenerManager* elm = GetOrCreateListenerManager();        \
    if (elm) {                                                                \
      elm->SetEventHandler(handler);                                          \
    }                                                                         \
  }
#define WINDOW_ONLY_EVENT EVENT
#define TOUCH_EVENT EVENT
#include "mozilla/EventNameList.h"
#undef TOUCH_EVENT
#undef WINDOW_ONLY_EVENT
#undef BEFOREUNLOAD_EVENT
#undef ERROR_EVENT
#undef EVENT

  nsISupports* GetParentObject()
  {
    return nullptr;
  }

  static JSObject*
    CreateNamedPropertiesObject(JSContext *aCx, JS::Handle<JSObject*> aProto);

  nsGlobalWindowInner* Window();
  nsGlobalWindowInner* Self();
  nsIDocument* GetDocument()
  {
    return GetDoc();
  }
  void GetName(nsAString& aName, mozilla::ErrorResult& aError);
  void SetName(const nsAString& aName, mozilla::ErrorResult& aError);
  mozilla::dom::Location* GetLocation() override;
  nsHistory* GetHistory(mozilla::ErrorResult& aError);
  mozilla::dom::CustomElementRegistry* CustomElements() override;
  mozilla::dom::BarProp* GetLocationbar(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetMenubar(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetPersonalbar(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetScrollbars(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetStatusbar(mozilla::ErrorResult& aError);
  mozilla::dom::BarProp* GetToolbar(mozilla::ErrorResult& aError);
  void GetStatus(nsAString& aStatus, mozilla::ErrorResult& aError);
  void SetStatus(const nsAString& aStatus, mozilla::ErrorResult& aError);
  void Close(mozilla::ErrorResult& aError);
  nsresult Close() override;
  bool GetClosed(mozilla::ErrorResult& aError);
  bool Closed() override;
  void Stop(mozilla::ErrorResult& aError);
  void Focus(mozilla::ErrorResult& aError);
  nsresult Focus() override;
  void Blur(mozilla::ErrorResult& aError);
  already_AddRefed<nsIDOMWindowCollection> GetFrames() override;
  already_AddRefed<nsPIDOMWindowOuter> GetFrames(mozilla::ErrorResult& aError);
  uint32_t Length();
  already_AddRefed<nsPIDOMWindowOuter> GetTop(mozilla::ErrorResult& aError);

  nsresult GetPrompter(nsIPrompt** aPrompt) override;
protected:
  explicit nsGlobalWindowInner(nsGlobalWindowOuter *aOuterWindow);
  // Initializes the mWasOffline member variable
  void InitWasOffline();
public:
  nsPIDOMWindowOuter* GetOpenerWindow(mozilla::ErrorResult& aError);
  void GetOpener(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval,
                 mozilla::ErrorResult& aError);
  already_AddRefed<nsPIDOMWindowOuter> GetOpener() override;
  void SetOpener(JSContext* aCx, JS::Handle<JS::Value> aOpener,
                 mozilla::ErrorResult& aError);
  already_AddRefed<nsPIDOMWindowOuter> GetParent(mozilla::ErrorResult& aError);
  already_AddRefed<nsPIDOMWindowOuter> GetParent() override;
  nsPIDOMWindowOuter* GetScriptableParent() override;
  nsPIDOMWindowOuter* GetScriptableParentOrNull() override;
  mozilla::dom::Element*
  GetFrameElement(nsIPrincipal& aSubjectPrincipal,
                  mozilla::ErrorResult& aError);
  already_AddRefed<nsIDOMElement> GetFrameElement() override;
  already_AddRefed<nsPIDOMWindowOuter>
  Open(const nsAString& aUrl,
       const nsAString& aName,
       const nsAString& aOptions,
       mozilla::ErrorResult& aError);
  nsresult Open(const nsAString& aUrl, const nsAString& aName,
                const nsAString& aOptions,
                nsIDocShellLoadInfo* aLoadInfo,
                bool aForceNoOpener,
                nsPIDOMWindowOuter **_retval) override;
  mozilla::dom::Navigator* Navigator();
  nsIDOMNavigator* GetNavigator() override;
  nsIDOMOfflineResourceList* GetApplicationCache(mozilla::ErrorResult& aError);
  already_AddRefed<nsIDOMOfflineResourceList> GetApplicationCache() override;

#if defined(MOZ_WIDGET_ANDROID)
  int16_t Orientation(mozilla::dom::CallerType aCallerType) const;
#endif

  mozilla::dom::Console* GetConsole(mozilla::ErrorResult& aRv);

  // https://w3c.github.io/webappsec-secure-contexts/#dom-window-issecurecontext
  bool IsSecureContext() const;
  bool IsSecureContextIfOpenerIgnored() const;

  void GetSidebar(mozilla::dom::OwningExternalOrWindowProxy& aResult,
                  mozilla::ErrorResult& aRv);
  already_AddRefed<mozilla::dom::External> GetExternal(mozilla::ErrorResult& aRv);

  // Exposed only for testing
  static bool
  TokenizeDialogOptions(nsAString& aToken, nsAString::const_iterator& aIter,
                        nsAString::const_iterator aEnd);
  static void
  ConvertDialogOptions(const nsAString& aOptions, nsAString& aResult);

  mozilla::dom::Worklet*
  GetAudioWorklet(mozilla::ErrorResult& aRv);

  mozilla::dom::Worklet*
  GetPaintWorklet(mozilla::ErrorResult& aRv);

  void
  GetRegionalPrefsLocales(nsTArray<nsString>& aLocales);

  mozilla::dom::IntlUtils*
  GetIntlUtils(mozilla::ErrorResult& aRv);

public:
  void Alert(nsIPrincipal& aSubjectPrincipal,
             mozilla::ErrorResult& aError);
  void Alert(const nsAString& aMessage,
             nsIPrincipal& aSubjectPrincipal,
             mozilla::ErrorResult& aError);
  bool Confirm(const nsAString& aMessage,
               nsIPrincipal& aSubjectPrincipal,
               mozilla::ErrorResult& aError);
  void Prompt(const nsAString& aMessage, const nsAString& aInitial,
              nsAString& aReturn,
              nsIPrincipal& aSubjectPrincipal,
              mozilla::ErrorResult& aError);
  already_AddRefed<mozilla::dom::cache::CacheStorage> GetCaches(mozilla::ErrorResult& aRv);
  already_AddRefed<mozilla::dom::Promise> Fetch(const mozilla::dom::RequestOrUSVString& aInput,
                                                const mozilla::dom::RequestInit& aInit,
                                                mozilla::dom::CallerType aCallerType,
                                                mozilla::ErrorResult& aRv);
  void Print(mozilla::ErrorResult& aError);
  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const nsAString& aTargetOrigin,
                      const mozilla::dom::Sequence<JSObject*>& aTransfer,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);
  int32_t SetTimeout(JSContext* aCx, mozilla::dom::Function& aFunction,
                     int32_t aTimeout,
                     const mozilla::dom::Sequence<JS::Value>& aArguments,
                     mozilla::ErrorResult& aError);
  int32_t SetTimeout(JSContext* aCx, const nsAString& aHandler,
                     int32_t aTimeout,
                     const mozilla::dom::Sequence<JS::Value>& /* unused */,
                     mozilla::ErrorResult& aError);
  void ClearTimeout(int32_t aHandle);
  int32_t SetInterval(JSContext* aCx, mozilla::dom::Function& aFunction,
                      const mozilla::dom::Optional<int32_t>& aTimeout,
                      const mozilla::dom::Sequence<JS::Value>& aArguments,
                      mozilla::ErrorResult& aError);
  int32_t SetInterval(JSContext* aCx, const nsAString& aHandler,
                      const mozilla::dom::Optional<int32_t>& aTimeout,
                      const mozilla::dom::Sequence<JS::Value>& /* unused */,
                      mozilla::ErrorResult& aError);
  void ClearInterval(int32_t aHandle);
  void GetOrigin(nsAString& aOrigin);
  void Atob(const nsAString& aAsciiBase64String, nsAString& aBinaryData,
            mozilla::ErrorResult& aError);
  void Btoa(const nsAString& aBinaryData, nsAString& aAsciiBase64String,
            mozilla::ErrorResult& aError);
  mozilla::dom::Storage* GetSessionStorage(mozilla::ErrorResult& aError);
  mozilla::dom::Storage*
  GetLocalStorage(mozilla::ErrorResult& aError);
  mozilla::dom::Selection* GetSelection(mozilla::ErrorResult& aError);
  already_AddRefed<nsISelection> GetSelection() override;
  mozilla::dom::IDBFactory* GetIndexedDB(mozilla::ErrorResult& aError);
  already_AddRefed<nsICSSDeclaration>
    GetComputedStyle(mozilla::dom::Element& aElt, const nsAString& aPseudoElt,
                     mozilla::ErrorResult& aError) override;
  already_AddRefed<mozilla::dom::MediaQueryList> MatchMedia(
    const nsAString& aQuery,
    mozilla::dom::CallerType aCallerType,
    mozilla::ErrorResult& aError);
  nsScreen* GetScreen(mozilla::ErrorResult& aError);
  nsIDOMScreen* GetScreen() override;
  void MoveTo(int32_t aXPos, int32_t aYPos,
              mozilla::dom::CallerType aCallerType,
              mozilla::ErrorResult& aError);
  void MoveBy(int32_t aXDif, int32_t aYDif,
              mozilla::dom::CallerType aCallerType,
              mozilla::ErrorResult& aError);
  nsresult MoveBy(int32_t aXDif, int32_t aYDif) override;
  void ResizeTo(int32_t aWidth, int32_t aHeight,
                mozilla::dom::CallerType aCallerType,
                mozilla::ErrorResult& aError);
  void ResizeBy(int32_t aWidthDif, int32_t aHeightDif,
                mozilla::dom::CallerType aCallerType,
                mozilla::ErrorResult& aError);
  void Scroll(double aXScroll, double aYScroll);
  void Scroll(const mozilla::dom::ScrollToOptions& aOptions);
  void ScrollTo(double aXScroll, double aYScroll);
  void ScrollTo(const mozilla::dom::ScrollToOptions& aOptions);
  void ScrollBy(double aXScrollDif, double aYScrollDif);
  void ScrollBy(const mozilla::dom::ScrollToOptions& aOptions);
  void ScrollByLines(int32_t numLines,
                     const mozilla::dom::ScrollOptions& aOptions);
  void ScrollByPages(int32_t numPages,
                     const mozilla::dom::ScrollOptions& aOptions);
  void MozScrollSnap();
  void GetInnerWidth(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void SetInnerWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void GetInnerHeight(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  void SetInnerHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  double GetScrollX(mozilla::ErrorResult& aError);
  double GetPageXOffset(mozilla::ErrorResult& aError)
  {
    return GetScrollX(aError);
  }
  double GetScrollY(mozilla::ErrorResult& aError);
  double GetPageYOffset(mozilla::ErrorResult& aError)
  {
    return GetScrollY(aError);
  }
  void GetScreenX(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  void SetScreenX(JSContext* aCx, JS::Handle<JS::Value> aValue,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  void GetScreenY(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  void SetScreenY(JSContext* aCx, JS::Handle<JS::Value> aValue,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  void GetOuterWidth(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void SetOuterWidth(JSContext* aCx, JS::Handle<JS::Value> aValue,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void GetOuterHeight(JSContext* aCx, JS::MutableHandle<JS::Value> aValue,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  void SetOuterHeight(JSContext* aCx, JS::Handle<JS::Value> aValue,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  int32_t RequestAnimationFrame(mozilla::dom::FrameRequestCallback& aCallback,
                                mozilla::ErrorResult& aError);
  void CancelAnimationFrame(int32_t aHandle, mozilla::ErrorResult& aError);

  uint32_t RequestIdleCallback(JSContext* aCx,
                               mozilla::dom::IdleRequestCallback& aCallback,
                               const mozilla::dom::IdleRequestOptions& aOptions,
                               mozilla::ErrorResult& aError);
  void CancelIdleCallback(uint32_t aHandle);

#ifdef MOZ_WEBSPEECH
  mozilla::dom::SpeechSynthesis*
    GetSpeechSynthesis(mozilla::ErrorResult& aError);
  bool HasActiveSpeechSynthesis();
#endif
  already_AddRefed<nsICSSDeclaration>
    GetDefaultComputedStyle(mozilla::dom::Element& aElt,
                            const nsAString& aPseudoElt,
                            mozilla::ErrorResult& aError);
  void SizeToContent(mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  mozilla::dom::Crypto* GetCrypto(mozilla::ErrorResult& aError);
  mozilla::dom::U2F* GetU2f(mozilla::ErrorResult& aError);
  nsIControllers* GetControllers(mozilla::ErrorResult& aError);
  nsresult GetControllers(nsIControllers** aControllers) override;
  mozilla::dom::Element* GetRealFrameElement(mozilla::ErrorResult& aError);
  float GetMozInnerScreenX(mozilla::dom::CallerType aCallerType,
                           mozilla::ErrorResult& aError);
  float GetMozInnerScreenY(mozilla::dom::CallerType aCallerType,
                           mozilla::ErrorResult& aError);
  double GetDevicePixelRatio(mozilla::dom::CallerType aCallerType,
                             mozilla::ErrorResult& aError);
  int32_t GetScrollMinX(mozilla::ErrorResult& aError);
  int32_t GetScrollMinY(mozilla::ErrorResult& aError);
  int32_t GetScrollMaxX(mozilla::ErrorResult& aError);
  int32_t GetScrollMaxY(mozilla::ErrorResult& aError);
  bool GetFullScreen(mozilla::ErrorResult& aError);
  bool GetFullScreen() override;
  void SetFullScreen(bool aFullScreen, mozilla::ErrorResult& aError);
  nsresult SetFullScreen(bool aFullScreen) override;
  void Back(mozilla::ErrorResult& aError);
  void Forward(mozilla::ErrorResult& aError);
  void Home(nsIPrincipal& aSubjectPrincipal, mozilla::ErrorResult& aError);
  bool Find(const nsAString& aString, bool aCaseSensitive, bool aBackwards,
            bool aWrapAround, bool aWholeWord, bool aSearchInFrames,
            bool aShowDialog, mozilla::ErrorResult& aError);
  uint64_t GetMozPaintCount(mozilla::ErrorResult& aError);

  bool ShouldResistFingerprinting();

  mozilla::dom::MozSelfSupport* GetMozSelfSupport(mozilla::ErrorResult& aError);

  already_AddRefed<nsPIDOMWindowOuter>
  OpenDialog(JSContext* aCx,
             const nsAString& aUrl,
             const nsAString& aName,
             const nsAString& aOptions,
             const mozilla::dom::Sequence<JS::Value>& aExtraArgument,
             mozilla::ErrorResult& aError);
  nsresult OpenDialog(const nsAString& aUrl, const nsAString& aName,
                      const nsAString& aOptions,
                      nsISupports* aExtraArgument,
                      nsPIDOMWindowOuter** _retval) override;
  nsresult UpdateCommands(const nsAString& anAction, nsISelection* aSel, int16_t aReason) override;

  void GetContent(JSContext* aCx,
                  JS::MutableHandle<JSObject*> aRetval,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);

  already_AddRefed<mozilla::dom::Promise>
  CreateImageBitmap(JSContext* aCx,
                    const mozilla::dom::ImageBitmapSource& aImage,
                    mozilla::ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise>
  CreateImageBitmap(JSContext* aCx,
                    const mozilla::dom::ImageBitmapSource& aImage,
                    int32_t aSx, int32_t aSy, int32_t aSw, int32_t aSh,
                    mozilla::ErrorResult& aRv);

  already_AddRefed<mozilla::dom::Promise>
  CreateImageBitmap(JSContext* aCx,
                    const mozilla::dom::ImageBitmapSource& aImage,
                    int32_t aOffset, int32_t aLength,
                    mozilla::dom::ImageBitmapFormat aFormat,
                    const mozilla::dom::Sequence<mozilla::dom::ChannelPixelLayout>& aLayout,
                    mozilla::ErrorResult& aRv);


  // ChromeWindow bits.  Do NOT call these unless your window is in
  // fact chrome.
  uint16_t WindowState();
  bool IsFullyOccluded();
  nsIBrowserDOMWindow* GetBrowserDOMWindow(mozilla::ErrorResult& aError);
  void SetBrowserDOMWindow(nsIBrowserDOMWindow* aBrowserWindow,
                           mozilla::ErrorResult& aError);
  void GetAttention(mozilla::ErrorResult& aError);
  void GetAttentionWithCycleCount(int32_t aCycleCount,
                                  mozilla::ErrorResult& aError);
  void SetCursor(const nsAString& aCursor, mozilla::ErrorResult& aError);
  void Maximize();
  void Minimize();
  void Restore();
  void NotifyDefaultButtonLoaded(mozilla::dom::Element& aDefaultButton,
                                 mozilla::ErrorResult& aError);
  nsIMessageBroadcaster* GetMessageManager(mozilla::ErrorResult& aError);
  nsIMessageBroadcaster* GetGroupMessageManager(const nsAString& aGroup,
                                                mozilla::ErrorResult& aError);
  void BeginWindowMove(mozilla::dom::Event& aMouseDownEvent,
                       mozilla::dom::Element* aPanel,
                       mozilla::ErrorResult& aError);

  void GetDialogArgumentsOuter(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval,
                               nsIPrincipal& aSubjectPrincipal,
                               mozilla::ErrorResult& aError);
  void GetDialogArguments(JSContext* aCx, JS::MutableHandle<JS::Value> aRetval,
                          nsIPrincipal& aSubjectPrincipal,
                          mozilla::ErrorResult& aError);
  void GetReturnValueOuter(JSContext* aCx, JS::MutableHandle<JS::Value> aReturnValue,
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& aError);
  void GetReturnValue(JSContext* aCx, JS::MutableHandle<JS::Value> aReturnValue,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);
  void SetReturnValueOuter(JSContext* aCx, JS::Handle<JS::Value> aReturnValue,
                           nsIPrincipal& aSubjectPrincipal,
                           mozilla::ErrorResult& aError);
  void SetReturnValue(JSContext* aCx, JS::Handle<JS::Value> aReturnValue,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);

  void GetInterface(JSContext* aCx, nsIJSID* aIID,
                    JS::MutableHandle<JS::Value> aRetval,
                    mozilla::ErrorResult& aError);

  already_AddRefed<nsWindowRoot> GetWindowRoot(mozilla::ErrorResult& aError);

  void UpdateTopInnerWindow();

  virtual bool IsInSyncOperation() override
  {
    return GetExtantDoc() && GetExtantDoc()->IsInSyncOperation();
  }

protected:
  // Web IDL helpers

  // Redefine the property called aPropName on this window object to be a value
  // property with the value aValue, much like we would do for a [Replaceable]
  // property in IDL.
  void RedefineProperty(JSContext* aCx, const char* aPropName,
                        JS::Handle<JS::Value> aValue,
                        mozilla::ErrorResult& aError);

  // Implementation guts for our writable IDL attributes that are really
  // supposed to be readonly replaceable.
  typedef int32_t
    (nsGlobalWindowInner::*WindowCoordGetter)(mozilla::dom::CallerType aCallerType,
                                              mozilla::ErrorResult&);
  typedef void
    (nsGlobalWindowInner::*WindowCoordSetter)(int32_t,
                                              mozilla::dom::CallerType aCallerType,
                                              mozilla::ErrorResult&);
  void GetReplaceableWindowCoord(JSContext* aCx, WindowCoordGetter aGetter,
                                 JS::MutableHandle<JS::Value> aRetval,
                                 mozilla::dom::CallerType aCallerType,
                                 mozilla::ErrorResult& aError);
  void SetReplaceableWindowCoord(JSContext* aCx, WindowCoordSetter aSetter,
                                 JS::Handle<JS::Value> aValue,
                                 const char* aPropName,
                                 mozilla::dom::CallerType aCallerType,
                                 mozilla::ErrorResult& aError);
  // And the implementations of WindowCoordGetter/WindowCoordSetter.
protected:
  int32_t GetInnerWidth(mozilla::dom::CallerType aCallerType,
                        mozilla::ErrorResult& aError);
  nsresult GetInnerWidth(int32_t* aWidth) override;
  void SetInnerWidth(int32_t aInnerWidth,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
protected:
  int32_t GetInnerHeight(mozilla::dom::CallerType aCallerType,
                         mozilla::ErrorResult& aError);
  nsresult GetInnerHeight(int32_t* aHeight) override;
  void SetInnerHeight(int32_t aInnerHeight,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);
  int32_t GetScreenX(mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void SetScreenX(int32_t aScreenX,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  int32_t GetScreenY(mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  void SetScreenY(int32_t aScreenY,
                  mozilla::dom::CallerType aCallerType,
                  mozilla::ErrorResult& aError);
  int32_t GetOuterWidth(mozilla::dom::CallerType aCallerType,
                        mozilla::ErrorResult& aError);
  void SetOuterWidth(int32_t aOuterWidth,
                     mozilla::dom::CallerType aCallerType,
                     mozilla::ErrorResult& aError);
  int32_t GetOuterHeight(mozilla::dom::CallerType aCallerType,
                         mozilla::ErrorResult& aError);
  void SetOuterHeight(int32_t aOuterHeight,
                      mozilla::dom::CallerType aCallerType,
                      mozilla::ErrorResult& aError);

  // Array of idle observers that are notified of idle events.
  nsTObserverArray<IdleObserverHolder> mIdleObservers;

  // Idle timer used for function callbacks to notify idle observers.
  nsCOMPtr<nsITimer> mIdleTimer;

  // Idle fuzz time added to idle timer callbacks.
  uint32_t mIdleFuzzFactor;

  // Index in mArrayIdleObservers
  // Next idle observer to notify user idle status
  int32_t mIdleCallbackIndex;

  // If false then the topic is "active"
  // If true then the topic is "idle"
  bool mCurrentlyIdle;

  // Set to true when a fuzz time needs to be applied
  // to active notifications to the idle observer.
  bool mAddActiveEventFuzzTime;

  nsCOMPtr <nsIIdleService> mIdleService;

  RefPtr<mozilla::dom::WakeLock> mWakeLock;

  friend class HashchangeCallback;
  friend class mozilla::dom::BarProp;

  // Object Management
  virtual ~nsGlobalWindowInner();
  void CleanUp();
  void ClearControllers();

  inline void MaybeClearInnerWindow(nsGlobalWindowInner* aExpectedInner);

  void FreeInnerObjects();
  nsGlobalWindowInner *CallerInnerWindow();

  // Only to be called on an inner window.
  // aDocument must not be null.
  void InnerSetNewDocument(JSContext* aCx, nsIDocument* aDocument);

  // Inner windows only.
  nsresult DefineArgumentsProperty(nsIArray *aArguments);

  // Get the parent, returns null if this is a toplevel window
  nsPIDOMWindowOuter* GetParentInternal();

public:
  // popup tracking
  bool IsPopupSpamWindow();

protected:
  // Window Control Functions

  // Outer windows only.
  virtual nsresult
  OpenNoNavigate(const nsAString& aUrl,
                 const nsAString& aName,
                 const nsAString& aOptions,
                 nsPIDOMWindowOuter** _retval) override;

private:
  template<typename Method>
  void CallOnChildren(Method aMethod);

  void FreezeInternal();
  void ThawInternal();

public:
  // Timeout Functions
  // |interval| is in milliseconds.
  int32_t SetTimeoutOrInterval(JSContext* aCx,
                               mozilla::dom::Function& aFunction,
                               int32_t aTimeout,
                               const mozilla::dom::Sequence<JS::Value>& aArguments,
                               bool aIsInterval, mozilla::ErrorResult& aError);
  int32_t SetTimeoutOrInterval(JSContext* aCx, const nsAString& aHandler,
                               int32_t aTimeout, bool aIsInterval,
                               mozilla::ErrorResult& aError);

  // Return true if |aTimeout| was cleared while its handler ran.
  bool RunTimeoutHandler(mozilla::dom::Timeout* aTimeout, nsIScriptContext* aScx);

  // Helper Functions
  already_AddRefed<nsIDocShellTreeOwner> GetTreeOwner();
  already_AddRefed<nsIWebBrowserChrome> GetWebBrowserChrome();
  bool IsPrivateBrowsing();

  void FireOfflineStatusEventIfChanged();

  bool GetIsPrerendered();

public:
  // Inner windows only.
  nsresult ScheduleNextIdleObserverCallback();
  uint32_t GetFuzzTimeMS();
  nsresult ScheduleActiveTimerCallback();
  uint32_t FindInsertionIndex(IdleObserverHolder* aIdleObserver);
  virtual nsresult RegisterIdleObserver(nsIIdleObserver* aIdleObserverPtr) override;
  nsresult FindIndexOfElementToRemove(nsIIdleObserver* aIdleObserver,
                                      int32_t* aRemoveElementIndex);
  virtual nsresult UnregisterIdleObserver(nsIIdleObserver* aIdleObserverPtr) override;

  // Inner windows only.
  nsresult FireHashchange(const nsAString &aOldURL, const nsAString &aNewURL);

  void FlushPendingNotifications(mozilla::FlushType aType);

  void ScrollTo(const mozilla::CSSIntPoint& aScroll,
                const mozilla::dom::ScrollOptions& aOptions);

  bool IsFrame();

  already_AddRefed<nsIWidget> GetMainWidget();
  nsIWidget* GetNearestWidget() const;

  bool IsInModalState();

  virtual void SetFocusedNode(nsIContent* aNode,
                              uint32_t aFocusMethod = 0,
                              bool aNeedsFocus = false) override;

  virtual uint32_t GetFocusMethod() override;

  virtual bool ShouldShowFocusRing() override;

  virtual void SetKeyboardIndicators(UIStateChangeType aShowAccelerators,
                                     UIStateChangeType aShowFocusRings) override;

  // Inner windows only.
  void UpdateCanvasFocus(bool aFocusChanged, nsIContent* aNewContent);

public:
  virtual already_AddRefed<nsPIWindowRoot> GetTopWindowRoot() override;

protected:
  static void NotifyDOMWindowDestroyed(nsGlobalWindowInner* aWindow);
  void NotifyWindowIDDestroyed(const char* aTopic);

  static void NotifyDOMWindowFrozen(nsGlobalWindowInner* aWindow);
  static void NotifyDOMWindowThawed(nsGlobalWindowInner* aWindow);

  virtual void UpdateParentTarget() override;

  void InitializeShowFocusRings();

  // Clear the document-dependent slots on our JS wrapper.  Inner windows only.
  void ClearDocumentDependentSlots(JSContext* aCx);

  // Inner windows only.
  already_AddRefed<mozilla::dom::StorageEvent>
  CloneStorageEvent(const nsAString& aType,
                    const RefPtr<mozilla::dom::StorageEvent>& aEvent,
                    mozilla::ErrorResult& aRv);

protected:
  already_AddRefed<nsICSSDeclaration>
    GetComputedStyleHelper(mozilla::dom::Element& aElt,
                           const nsAString& aPseudoElt,
                           bool aDefaultStylesOnly,
                           mozilla::ErrorResult& aError);
  nsresult GetComputedStyleHelper(nsIDOMElement* aElt,
                                  const nsAString& aPseudoElt,
                                  bool aDefaultStylesOnly,
                                  nsIDOMCSSStyleDeclaration** aReturn);

  nsGlobalWindowInner* InnerForSetTimeoutOrInterval(mozilla::ErrorResult& aError);

  void PostMessageMoz(JSContext* aCx, JS::Handle<JS::Value> aMessage,
                      const nsAString& aTargetOrigin,
                      JS::Handle<JS::Value> aTransfer,
                      nsIPrincipal& aSubjectPrincipal,
                      mozilla::ErrorResult& aError);

private:
  // Fire the JS engine's onNewGlobalObject hook.  Only used on inner windows.
  void FireOnNewGlobalObject();

  void DisconnectEventTargetObjects();

  // nsPIDOMWindow<T> should be able to see these helper methods.
  friend class nsPIDOMWindow<mozIDOMWindowProxy>;
  friend class nsPIDOMWindow<mozIDOMWindow>;
  friend class nsPIDOMWindow<nsISupports>;
  friend class nsPIDOMWindowInner;
  friend class nsPIDOMWindowOuter;

  mozilla::dom::TabGroup* TabGroupInner();

  bool IsBackgroundInternal() const;

  // NOTE: Chrome Only
  void DisconnectAndClearGroupMessageManagers()
  {
    MOZ_RELEASE_ASSERT(IsChromeWindow());
    for (auto iter = mChromeFields.mGroupMessageManagers.Iter(); !iter.Done(); iter.Next()) {
      nsIMessageBroadcaster* mm = iter.UserData();
      if (mm) {
        static_cast<nsFrameMessageManager*>(mm)->Disconnect();
      }
    }
    mChromeFields.mGroupMessageManagers.Clear();
  }

public:
  // Dispatch a runnable related to the global.
  virtual nsresult Dispatch(mozilla::TaskCategory aCategory,
                            already_AddRefed<nsIRunnable>&& aRunnable) override;

  virtual nsISerialEventTarget*
  EventTargetFor(mozilla::TaskCategory aCategory) const override;

  virtual mozilla::AbstractThread*
  AbstractMainThreadFor(mozilla::TaskCategory aCategory) override;

  void DisableIdleCallbackRequests();
  uint32_t LastIdleRequestHandle() const { return mIdleRequestCallbackCounter - 1; }
  nsresult RunIdleRequest(mozilla::dom::IdleRequest* aRequest,
                          DOMHighResTimeStamp aDeadline, bool aDidTimeout);
  nsresult ExecuteIdleRequest(TimeStamp aDeadline);
  void ScheduleIdleRequestDispatch();
  void SuspendIdleRequests();
  void ResumeIdleRequests();

  typedef mozilla::LinkedList<mozilla::dom::IdleRequest> IdleRequests;
  void InsertIdleCallback(mozilla::dom::IdleRequest* aRequest);

  void RemoveIdleCallback(mozilla::dom::IdleRequest* aRequest);

protected:
  // These members are only used on outer window objects. Make sure
  // you never set any of these on an inner object!
  bool                          mFullScreen : 1;
  bool                          mFullscreenMode : 1;
  bool                          mIsClosed : 1;
  bool                          mInClose : 1;
  // mHavePendingClose means we've got a termination function set to
  // close us when the JS stops executing or that we have a close
  // event posted.  If this is set, just ignore window.close() calls.
  bool                          mHavePendingClose : 1;
  bool                          mHadOriginalOpener : 1;
  bool                          mOriginalOpenerWasSecureContext : 1;
  bool                          mIsSecureContextIfOpenerIgnored : 1;
  bool                          mIsPopupSpam : 1;

  // Indicates whether scripts are allowed to close this window.
  bool                          mBlockScriptedClosingFlag : 1;

  // Window offline status. Checked to see if we need to fire offline event
  bool                          mWasOffline : 1;

  // Represents whether the inner window's page has had a slow script notice.
  // Only used by inner windows; will always be false for outer windows.
  // This is used to implement Telemetry measures such as SLOW_SCRIPT_PAGE_COUNT.
  bool                          mHasHadSlowScript : 1;

  // Track what sorts of events we need to fire when thawed
  bool                          mNotifyIdleObserversIdleOnThaw : 1;
  bool                          mNotifyIdleObserversActiveOnThaw : 1;

  // Indicates whether we're in the middle of creating an initializing
  // a new inner window object.
  bool                          mCreatingInnerWindow : 1;

  // Fast way to tell if this is a chrome window (without having to QI).
  bool                          mIsChrome : 1;

  // Hack to indicate whether a chrome window needs its message manager
  // to be disconnected, since clean up code is shared in the global
  // window superclass.
  bool                          mCleanMessageManager : 1;

  // Indicates that the current document has never received a document focus
  // event.
  bool                   mNeedsFocus : 1;
  bool                   mHasFocus : 1;

  // when true, show focus rings for the current focused content only.
  // This will be reset when another element is focused
  bool                   mShowFocusRingForContent : 1;

  // true if tab navigation has occurred for this window. Focus rings
  // should be displayed.
  bool                   mFocusByKeyOccurred : 1;

  // Inner windows only.
  // Indicates whether this window wants gamepad input events
  bool                   mHasGamepad : 1;

  // Inner windows only.
  // Indicates whether this window wants VR events
  bool                   mHasVREvents : 1;

  // Inner windows only.
  // Indicates whether this window wants VRDisplayActivate events
  bool                   mHasVRDisplayActivateEvents : 1;
  nsCheapSet<nsUint32HashKey> mGamepadIndexSet;
  nsRefPtrHashtable<nsUint32HashKey, mozilla::dom::Gamepad> mGamepads;
  bool mHasSeenGamepadInput;

  // whether we've sent the destroy notification for our window id
  bool                   mNotifiedIDDestroyed : 1;
  // whether scripts may close the window,
  // even if "dom.allow_scripts_to_close_windows" is false.
  bool                   mAllowScriptsToClose : 1;

  bool mTopLevelOuterContentWindow : 1;

  nsCOMPtr<nsIScriptContext>    mContext;
  nsWeakPtr                     mOpener;
  nsCOMPtr<nsIControllers>      mControllers;

  // For |window.arguments|, via |openDialog|.
  nsCOMPtr<nsIArray>            mArguments;

  // Only used in the outer.
  RefPtr<DialogValueHolder> mReturnValue;

  RefPtr<mozilla::dom::Navigator> mNavigator;
  RefPtr<nsScreen>            mScreen;
  RefPtr<nsDOMWindowList>     mFrames;
  // All BarProps are inner window only.
  RefPtr<mozilla::dom::BarProp> mMenubar;
  RefPtr<mozilla::dom::BarProp> mToolbar;
  RefPtr<mozilla::dom::BarProp> mLocationbar;
  RefPtr<mozilla::dom::BarProp> mPersonalbar;
  RefPtr<mozilla::dom::BarProp> mStatusbar;
  RefPtr<mozilla::dom::BarProp> mScrollbars;
  RefPtr<nsDOMWindowUtils>      mWindowUtils;
  nsString                      mStatus;
  nsString                      mDefaultStatus;
  RefPtr<nsGlobalWindowObserver> mObserver; // Inner windows only.
  RefPtr<mozilla::dom::Crypto>  mCrypto;
  RefPtr<mozilla::dom::U2F> mU2F;
  RefPtr<mozilla::dom::cache::CacheStorage> mCacheStorage;
  RefPtr<mozilla::dom::Console> mConsole;
  RefPtr<mozilla::dom::Worklet> mAudioWorklet;
  RefPtr<mozilla::dom::Worklet> mPaintWorklet;
  // We need to store an nsISupports pointer to this object because the
  // mozilla::dom::External class doesn't exist on b2g and using the type
  // forward declared here means that ~nsGlobalWindow wouldn't compile because
  // it wouldn't see the ~External function's declaration.
  nsCOMPtr<nsISupports>         mExternal;

  RefPtr<mozilla::dom::MozSelfSupport> mMozSelfSupport;

  RefPtr<mozilla::dom::Storage> mLocalStorage;
  RefPtr<mozilla::dom::Storage> mSessionStorage;

  // These member variable are used only on inner windows.
  RefPtr<mozilla::EventListenerManager> mListenerManager;
  RefPtr<mozilla::dom::Location> mLocation;
  RefPtr<nsHistory>           mHistory;
  RefPtr<mozilla::dom::CustomElementRegistry> mCustomElements;

  // These member variables are used on both inner and the outer windows.
  nsCOMPtr<nsIPrincipal> mDocumentPrincipal;
  // mTabChild is only ever populated in the content process.
  nsCOMPtr<nsITabChild>  mTabChild;

  uint32_t mSuspendDepth;
  uint32_t mFreezeDepth;

  // the method that was used to focus mFocusedNode
  uint32_t mFocusMethod;

  uint32_t mSerial;

  // The current idle request callback handle
  uint32_t mIdleRequestCallbackCounter;
  IdleRequests mIdleRequestCallbacks;
  RefPtr<IdleRequestExecutor> mIdleRequestExecutor;

#ifdef DEBUG
  bool mSetOpenerWindowCalled;
  nsCOMPtr<nsIURI> mLastOpenedURI;
#endif

  bool mCleanedUp;

  nsCOMPtr<nsIDOMOfflineResourceList> mApplicationCache;

  using XBLPrototypeHandlerTable = nsJSThingHashtable<nsPtrHashKey<nsXBLPrototypeHandler>, JSObject*>;
  mozilla::UniquePtr<XBLPrototypeHandlerTable> mCachedXBLPrototypeHandlers;

  // mSuspendedDoc is only set on outer windows. It's useful when we get matched
  // EnterModalState/LeaveModalState calls, in which case the outer window is
  // responsible for unsuspending events on the document. If we don't (for
  // example, if the outer window is closed before the LeaveModalState call),
  // then the inner window whose mDoc is our mSuspendedDoc is responsible for
  // unsuspending it.
  nsCOMPtr<nsIDocument> mSuspendedDoc;

  RefPtr<mozilla::dom::IDBFactory> mIndexedDB;

  // This counts the number of windows that have been opened in rapid succession
  // (i.e. within dom.successive_dialog_time_limit of each other). It is reset
  // to 0 once a dialog is opened after dom.successive_dialog_time_limit seconds
  // have elapsed without any other dialogs.
  uint32_t                      mDialogAbuseCount;

  // This holds the time when the last modal dialog was shown. If more than
  // MAX_DIALOG_LIMIT dialogs are shown within the time span defined by
  // dom.successive_dialog_time_limit, we show a checkbox or confirmation prompt
  // to allow disabling of further dialogs from this window.
  TimeStamp                     mLastDialogQuitTime;

  // This flag keeps track of whether dialogs are
  // currently enabled on this window.
  bool                          mAreDialogsEnabled;

  nsTHashtable<nsPtrHashKey<mozilla::DOMEventTargetHelper> > mEventTargetObjects;

  nsTArray<uint32_t> mEnabledSensors;

#if defined(MOZ_WIDGET_ANDROID)
  mozilla::UniquePtr<mozilla::dom::WindowOrientationObserver> mOrientationChangeObserver;
#endif

#ifdef MOZ_WEBSPEECH
  // mSpeechSynthesis is only used on inner windows.
  RefPtr<mozilla::dom::SpeechSynthesis> mSpeechSynthesis;
#endif

#ifdef DEBUG
  // This member is used in the debug only assertions in TabGroup()
  // to catch cyclic parent/opener trees and not overflow the stack.
  bool mIsValidatingTabGroup;
#endif

  // This is the CC generation the last time we called CanSkip.
  uint32_t mCanSkipCCGeneration;

  // The VR Displays for this window
  nsTArray<RefPtr<mozilla::dom::VRDisplay>> mVRDisplays;

  RefPtr<mozilla::dom::VREventObserver> mVREventObserver;

  // When non-zero, the document should receive a vrdisplayactivate event
  // after loading.  The value is the ID of the VRDisplay that content should
  // begin presentation on.
  uint32_t mAutoActivateVRDisplayID; // Outer windows only
  int64_t mBeforeUnloadListenerCount; // Inner windows only

  RefPtr<mozilla::dom::IntlUtils> mIntlUtils;

  static InnerWindowByIdTable* sInnerWindowsById;

  // Members in the mChromeFields member should only be used in chrome windows.
  // All accesses to this field should be guarded by a check of mIsChrome.
  struct ChromeFields {
    ChromeFields()
      : mGroupMessageManagers(1)
    {}

    nsCOMPtr<nsIBrowserDOMWindow> mBrowserDOMWindow;
    nsCOMPtr<nsIMessageBroadcaster> mMessageManager;
    nsInterfaceHashtable<nsStringHashKey, nsIMessageBroadcaster> mGroupMessageManagers;
    // A weak pointer to the nsPresShell that we are doing fullscreen for.
    // The pointer being set indicates we've set the IsInFullscreenChange
    // flag on this pres shell.
    nsWeakPtr mFullscreenPresShell;
    nsCOMPtr<mozIDOMWindowProxy> mOpenerForInitialContentBrowser;
  } mChromeFields;

  friend class nsDOMScriptableHelper;
  friend class nsDOMWindowUtils;
  friend class mozilla::dom::PostMessageEvent;
  friend class DesktopNotification;
  friend class mozilla::dom::TimeoutManager;
  friend class IdleRequestExecutor;
  friend class nsGlobalWindowOuter;
};

inline nsISupports*
ToSupports(nsGlobalWindowInner *p)
{
    return static_cast<nsIDOMEventTarget*>(p);
}

inline nsISupports*
ToCanonicalSupports(nsGlobalWindowInner *p)
{
    return static_cast<nsIDOMEventTarget*>(p);
}

// XXX: EWW - This is an awful hack - let's not do this
#include "nsGlobalWindowOuter.h"

inline nsIGlobalObject*
nsGlobalWindowInner::GetOwnerGlobal() const
{
  return const_cast<nsGlobalWindowInner*>(this);
}

inline nsGlobalWindowOuter*
nsGlobalWindowInner::GetTopInternal()
{
  nsGlobalWindowOuter* outer = GetOuterWindowInternal();
  nsCOMPtr<nsPIDOMWindowOuter> top = outer ? outer->GetTop() : nullptr;
  if (top) {
    return nsGlobalWindowOuter::Cast(top);
  }
  return nullptr;
}

inline nsGlobalWindowOuter*
nsGlobalWindowInner::GetScriptableTopInternal()
{
  nsPIDOMWindowOuter* top = GetScriptableTop();
  return nsGlobalWindowOuter::Cast(top);
}

inline nsIScriptContext*
nsGlobalWindowInner::GetContextInternal()
{
  if (mOuterWindow) {
    return GetOuterWindowInternal()->mContext;
  }

  return mContext;
}

inline nsGlobalWindowOuter*
nsGlobalWindowInner::GetOuterWindowInternal() const
{
  return nsGlobalWindowOuter::Cast(GetOuterWindow());
}

inline bool
nsGlobalWindowInner::IsPopupSpamWindow()
{
  if (IsInnerWindow() && !mOuterWindow) {
    return false;
  }

  return GetOuterWindowInternal()->mIsPopupSpam;
}

inline bool
nsGlobalWindowInner::IsFrame()
{
  return GetParentInternal() != nullptr;
}

inline void
nsGlobalWindowInner::MaybeClearInnerWindow(nsGlobalWindowInner* aExpectedInner)
{
  if(mInnerWindow == aExpectedInner->AsInner()) {
    mInnerWindow = nullptr;
  }
}

#endif /* nsGlobalWindowInner_h___ */
