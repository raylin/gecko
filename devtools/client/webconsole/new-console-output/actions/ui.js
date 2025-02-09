/* -*- indent-tabs-mode: nil; js-indent-level: 2 -*- */
/* vim: set ft=javascript ts=2 et sw=2 tw=80: */
/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/. */

"use strict";

const { getAllUi } = require("devtools/client/webconsole/new-console-output/selectors/ui");
const Services = require("Services");

const {
  FILTER_BAR_TOGGLE,
  INITIALIZE,
  PERSIST_TOGGLE,
  PREFS,
  SELECT_NETWORK_MESSAGE_TAB,
  TIMESTAMPS_TOGGLE,
} = require("devtools/client/webconsole/new-console-output/constants");

function filterBarToggle(show) {
  return (dispatch, getState) => {
    dispatch({
      type: FILTER_BAR_TOGGLE,
    });
    const uiState = getAllUi(getState());
    Services.prefs.setBoolPref(PREFS.UI.FILTER_BAR, uiState.get("filterBarVisible"));
  };
}

function persistToggle(show) {
  return (dispatch, getState) => {
    dispatch({
      type: PERSIST_TOGGLE,
    });
    const uiState = getAllUi(getState());
    Services.prefs.setBoolPref(PREFS.UI.PERSIST, uiState.get("persistLogs"));
  };
}

function timestampsToggle(visible) {
  return {
    type: TIMESTAMPS_TOGGLE,
    visible,
  };
}

function selectNetworkMessageTab(id) {
  return {
    type: SELECT_NETWORK_MESSAGE_TAB,
    id,
  };
}

function initialize() {
  return {
    type: INITIALIZE
  };
}

module.exports = {
  filterBarToggle,
  initialize,
  persistToggle,
  selectNetworkMessageTab,
  timestampsToggle,
};
