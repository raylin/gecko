"use strict";

const PAGE = "https://example.com/browser/toolkit/content/tests/browser/seek_with_sound.ogg";
const { classes: Cc, interfaces: Ci, utils: Cu } = Components;

add_task(function* () {
  yield BrowserTestUtils.withNewTab({
    gBrowser,
    url: PAGE
  }, testVideoControls);
});

function* testVideoControls(browser) {

  function getAnonElemByAttr(boundElm, attr, val) {
    return boundElm.ownerDocument.getAnonymousElementByAttribute(
      videoControl, attr, val); 
  }

  const video = content.document.getElementsByTagName("video")[0];
  const browserMM = browser.messageManager;

  let videoControl;
  let controlBar;

  const videoWidth = 320;
  const videoHeight = 240;

  const controlBarHeight = 28;
  const controlBarCenterY = videoHeight - Math.round(controlBarHeight / 2);

  // control order: play|scrubber|duration|mute|volume|cc|fullscreen
  const playButtonWidth = 28;
  const muteButtonWidth = 33;
  const fullscreenButtonWidth = 28;
  const volumeSliderWidth = 32;

  const playButtonCenterX = Math.round(playButtonWidth / 2);
  const fullscreenButtonCenterX = videoWidth - Math.round(fullscreenButtonWidth / 2);
  const muteButtonCenterX = videoWidth - fullscreenButtonWidth - volumeSliderWidth - Math.round(muteButtonWidth / 2);

  browserMM.addMessageListener("vidctrl-ping", msg => {
    videoControl = msg.objects.videocontrol; 
    controlBar = getAnonElemByAttr(videoControl, "class", "controlBar");

    browserMM.sendAsyncMessage("vidctrl-pong");
  });

  yield ContentTask.spawn(browser, {}, function* () {
    const video = content.document.getElementsByTagName("video")[0];
    const domUtil = Cc["@mozilla.org/inspector/dom-utils;1"].getService(Ci.inIDOMUtils);
    const videoControl = domUtil.getChildrenForNode(video, true)[1];

    yield new Promise(resolve => {
      sendAsyncMessage("vidctrl-ping", {}, {videocontrol: videoControl});
      addMessageListener("vidctrl-pong", resolve);
    });
  });

  video.setAttribute("mozNoDynamicControls", "true");
  video.removeAttribute("hidden");

  yield new Promise(resolve => {
    content.document.addEventListener("mozfullscreenchange", () => {
      ok(true);
      BrowserTestUtils.synthesizeKey("VK_ESCAPE", {}, browser);
      resolve();
    });

    BrowserTestUtils.synthesizeMouse(videoControl, fullscreenButtonCenterX, controlBarCenterY, {}, browser);
  });

  yield BrowserTestUtils.waitForEvent(content.document, "mozfullscreenchange");
  yield BrowserTestUtils.waitForCondition(() => {
    return !videoControl.hasAttribute("hidden");
  });

  yield new Promise(resolve => {
    video.addEventListener("pause", () => {
      ok(true, "should pause");
      resolve();
    });

    BrowserTestUtils.synthesizeMouse(videoControl, playButtonCenterX, controlBarCenterY, {}, browser);
  });

  yield new Promise(resolve => {
    video.addEventListener("volumechange", () => {
      ok(video.muted, "should muted");
      resolve();
    });

    BrowserTestUtils.synthesizeMouse(videoControl, muteButtonCenterX, controlBarCenterY, {}, browser);
  });
}
