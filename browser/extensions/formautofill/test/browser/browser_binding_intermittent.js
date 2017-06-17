"use strict";

const FORM_URL = "http://mochi.test:8888/browser/browser/extensions/formautofill/test/browser/autocomplete_basic.html";

add_task(async function setup_storage() {
  await saveAddress(TEST_ADDRESS_1);
  await saveAddress(TEST_ADDRESS_2);
  await saveAddress(TEST_ADDRESS_3);
});

registerCleanupFunction(async function() {
  let addresses = await getAddresses();
  if (addresses.length) {
    await removeAddresses(addresses.map(address => address.guid));
  }
});

add_task(async function test_binding_attachment() {
  await BrowserTestUtils.withNewTab({gBrowser, url: FORM_URL}, async function(browser) {
    const {autoCompletePopup, autoCompletePopup: {richlistbox: itemsBox}} = browser;

    await ContentTask.spawn(browser, {}, async function() {
      content.document.getElementById("street-address").focus();
    });
    await new Promise(resolve => setTimeout(resolve, 1000));
    await BrowserTestUtils.synthesizeKey("VK_DOWN", {}, browser);
    await BrowserTestUtils.waitForCondition(() => autoCompletePopup.popupOpen);
    ok(true, "Popup opened");

    const listItemElems = itemsBox.querySelectorAll(".autocomplete-richlistitem");
    for (let item of listItemElems) {
      is(item.getAttribute("originaltype"), "autofill-profile", "Checking originaltype");
      is(item.getAttribute("actualtype"), "formautofill", "Checking the item has actually attached");
    }
  });
});
