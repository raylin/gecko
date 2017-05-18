/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

/*
 * Defines a handler object to represent forms that autofill can handle.
 */

"use strict";

this.EXPORTED_SYMBOLS = ["FormAutofillHandler"];

const {classes: Cc, interfaces: Ci, utils: Cu, results: Cr} = Components;

Cu.import("resource://gre/modules/XPCOMUtils.jsm");

Cu.import("resource://formautofill/FormAutofillUtils.jsm");

XPCOMUtils.defineLazyModuleGetter(this, "FormAutofillHeuristics",
                                  "resource://formautofill/FormAutofillHeuristics.jsm");

this.log = null;
FormAutofillUtils.defineLazyLogGetter(this, this.EXPORTED_SYMBOLS[0]);

/**
 * Handles profile autofill for a DOM Form element.
 * @param {FormLike} form Form that need to be auto filled
 */
function FormAutofillHandler(form) {
  this.form = form;
  this.fieldDetails = [];
  this.winUtils = this.form.rootElement.ownerGlobal.QueryInterface(Ci.nsIInterfaceRequestor)
    .getInterface(Ci.nsIDOMWindowUtils);
}

FormAutofillHandler.prototype = {
  /**
   * DOM Form element to which this object is attached.
   */
  form: null,

  /**
   * Array of collected data about relevant form fields.  Each item is an object
   * storing the identifying details of the field and a reference to the
   * originally associated element from the form.
   *
   * The "section", "addressType", "contactType", and "fieldName" values are
   * used to identify the exact field when the serializable data is received
   * from the backend.  There cannot be multiple fields which have
   * the same exact combination of these values.
   *
   * A direct reference to the associated element cannot be sent to the user
   * interface because processing may be done in the parent process.
   */
  fieldDetails: null,

  /**
   * String of the filled profile's guid.
   */
  filledProfileGUID: null,

  /**
   * A WindowUtils reference of which Window the form belongs
   */
  winUtils: null,

  /**
   * Enum for form autofill MANUALLY_MANAGED_STATES values
   */
  fieldStateEnum: {
    // not themed
    NORMAL: null,
    // highlighted
    AUTO_FILLED: "-moz-autofill",
    // highlighted && grey color text
    PREVIEW: "-moz-autofill-preview",
  },

  /**
   * Set fieldDetails from the form about fields that can be autofilled.
   */
  collectFormFields() {
    let fieldDetails = FormAutofillHeuristics.getFormInfo(this.form);
    this.fieldDetails = fieldDetails ? fieldDetails : [];
    log.debug("Collected details on", this.fieldDetails.length, "fields");
  },

  /**
   * Processes form fields that can be autofilled, and populates them with the
   * profile provided by backend.
   *
   * @param {Object} profile
   *        A profile to be filled in.
   * @param {Object} focusedInput
   *        A focused input element which is skipped for filling.
   */
  autofillFormFields(profile, focusedInput) {
    log.debug("profile in autofillFormFields:", profile);

    this.filledProfileGUID = profile.guid;
    for (let fieldDetail of this.fieldDetails) {
      // Avoid filling field value in the following cases:
      // 1. the focused input which is filled in FormFillController.
      // 2. a non-empty input field
      // 3. the invalid value set

      let element = fieldDetail.elementWeakRef.get();
      if (!element || element.value) {
        continue;
      }

      let value = profile[fieldDetail.fieldName];
      if (value) {
        if (element !== focusedInput) {
          element.setUserInput(value);
        }

        this.transitionFieldState(fieldDetail, "AUTO_FILLED");
      }
    }

    // SetUserInput asynchronously dispatches input event, so we add a one time
    // listener for auto-filled first and then regisiter another listener to watch
    // if any modification made by user and be in charge of hightlight handling.
    this.form.rootElement.addEventListener("input", e => {
      log.debug("register change handler for auto-filled form:", this.form);

      const onChangeHandler = e => {
        let filledCount = 0;

        for (let fieldDetail of this.fieldDetails) {
          let element = fieldDetail.elementWeakRef.get();

          if (e.target === element || e.type === "reset") {
            this.transitionFieldState(fieldDetail, "NORMAL");
          }

          if (fieldDetail.state === "AUTO_FILLED") {
            filledCount++;
          }
        };

        // unregister listeners once no fields is in AUTO_FILLED state.
        if (filledCount === 0) {
          this.form.rootElement.removeEventListener("input", onChangeHandler, {mozSystemGroup: true});
          this.form.rootElement.removeEventListener("reset", onChangeHandler, {mozSystemGroup: true});
        }
      }

      this.form.rootElement.addEventListener("input", onChangeHandler, {mozSystemGroup: true});
      this.form.rootElement.addEventListener("reset", onChangeHandler, {mozSystemGroup: true});
    }, {mozSystemGroup: true, once: true});
  },

  /**
   * Populates result to the preview layers with given profile.
   *
   * @param {Object} profile
   *        A profile to be previewed with
   */
  previewFormFields(profile) {
    log.debug("preview profile in autofillFormFields:", profile);

    for (let fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();
      let value = profile[fieldDetail.fieldName] || "";

      // Skip the field that is null or already has text entered
      if (!element || element.value) {
        continue;
      }

      element.previewValue = value;
      this.transitionFieldState(fieldDetail, value ? "PREVIEW": "NORMAL");
    }
  },

  /**
   * Clear preview text and background highlight of all fields.
   */
  clearPreviewedFormFields() {
    log.debug("clear previewed fields in:", this.form);

    for (let fieldDetail of this.fieldDetails) {
      let element = fieldDetail.elementWeakRef.get();

      element.previewValue = "";

      // We keep the state if this field has
      // already been auto-filled.
      if (fieldDetail.state === "AUTO_FILLED") {
        continue;
      }

      this.transitionFieldState(fieldDetail, "NORMAL")
    }
  },

  /**
   * Transition the state of a field to correspond with different presentations.
   *
   * @param {Object} fieldDetail
   *        A fieldDetail of which its element is about to update the state.
   * @param {string} nextState
   *        Used to determine the next state to transition.
   */
  transitionFieldState(fieldDetail, nextState) {
    let element = fieldDetail.elementWeakRef.get();

    if (!element || !(nextState in this.fieldStateEnum)) {
      log.warn(fieldDetail.fieldName, "is unreachable while state transition");
      return;
    }

    for (let state in this.fieldStateEnum) {
      let mmState = this.fieldStateEnum[state];

      // Do nothing if mmState is null, i.e. NORMAL state
      if (!mmState) {
        continue;
      }

      if (state === nextState) {
        this.winUtils.addManuallyManagedState(element, mmState);
      } else {
        this.winUtils.removeManuallyManagedState(element, mmState);
      }
    }

    fieldDetail.state = nextState;
  },
};
