/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

const FormHistory = (Components.utils.import("resource://gre/modules/FormHistory.jsm", {})).FormHistory;


async function sendKeyEventToBrowser(browser, keyCode, modifier = null) {
  await ContentTask.spawn(browser, {keyCode, modifier}, async function({keyCode, modifier}) {
    const utils = content.QueryInterface(Components.interfaces.nsIInterfaceRequestor)
                         .getInterface(Components.interfaces.nsIDOMWindowUtils);
    const key = Ci.nsIDOMKeyEvent[keyCode];

    if (utils.sendKeyEvent("keydown", key, 0, modifier)) {
      utils.sendKeyEvent("keypress", key, 0, modifier);
    }
    utils.sendKeyEvent("keyup", key, 0, modifier);
  });
}

add_task(async function test() {
  const tab = await BrowserTestUtils.openNewForegroundTab(gBrowser, `data:text/html,<input type="text" name="field1">`, false);
  const browser = gBrowser.selectedBrowser;
  const {autoCompletePopup, autoCompletePopup: {richlistbox: itemsBox}} = browser;
  const mockHistory = [
    {op: "add", fieldname: "field1", value: "value1"},
    {op: "add", fieldname: "field1", value: "value2"},
    {op: "add", fieldname: "field1", value: "value3"},
    {op: "add", fieldname: "field1", value: "value4"},
  ];

  await new Promise(resolve => FormHistory.update([{op: "remove"}, ...mockHistory], {handleCompletion: resolve}));
  await ContentTask.spawn(browser, {}, async function() {
    const input = content.document.querySelector("input");

    input.focus();
  });

  // show popup
  await sendKeyEventToBrowser(browser, "DOM_VK_DOWN");
  await BrowserTestUtils.waitForCondition(() => {
    return autoCompletePopup.popupOpen;
  });
  const listItemElems = itemsBox.querySelectorAll(".autocomplete-richlistitem");
  is(listItemElems.length, mockHistory.length, "ensure result length");
  is(itemsBox.mousedOverIndex, -1, "mousedOverIndex should be -1");

  // navigate to the firt item
  await sendKeyEventToBrowser(browser, "DOM_VK_DOWN");
  is(autoCompletePopup.selectedIndex, 0, "selectedIndex should be 0");

  // mouseover the second item
  EventUtils.synthesizeMouseAtCenter(listItemElems[1], {type: "mouseover"});
  await BrowserTestUtils.waitForCondition(() => {
    return itemsBox.mousedOverIndex = 1;
  });
  ok(true, "mousedOverIndex changed");
  is(autoCompletePopup.selectedIndex, 0, "selectedIndex should not be changed by mouseover");

  await BrowserTestUtils.removeTab(tab);
});
