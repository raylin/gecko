/* Any copyright is dedicated to the Public Domain.
   http://creativecommons.org/publicdomain/zero/1.0/ */

/**
 * Test the helper functions of the shapes highlighter.
 */

"use strict";

const {
  splitCoords,
  coordToPercent,
  evalCalcExpression,
  shapeModeToCssPropertyName,
  getCirclePath,
  getUnit
} = require("devtools/server/actors/highlighters/shapes");
const { scalePoint } = require("devtools/server/actors/utils/shapes-utils");

function run_test() {
  test_split_coords();
  test_coord_to_percent();
  test_eval_calc_expression();
  test_shape_mode_to_css_property_name();
  test_get_circle_path();
  test_get_unit();
  test_scale_point();
  run_next_test();
}

function test_split_coords() {
  const tests = [{
    desc: "splitCoords for basic coordinate pair",
    expr: "30% 20%",
    expected: ["30%", "20%"]
  }, {
    desc: "splitCoords for coord pair with calc()",
    expr: "calc(50px + 20%) 30%",
    expected: ["calc(50px\u00a0+\u00a020%)", "30%"]
  }];

  for (let { desc, expr, expected } of tests) {
    deepEqual(splitCoords(expr), expected, desc);
  }
}

function test_coord_to_percent() {
  const size = 1000;
  const tests = [{
    desc: "coordToPercent for percent value",
    expr: "50%",
    expected: 50
  }, {
    desc: "coordToPercent for px value",
    expr: "500px",
    expected: 50
  }, {
    desc: "coordToPercent for zero value",
    expr: "0",
    expected: 0
  }];

  for (let { desc, expr, expected } of tests) {
    equal(coordToPercent(expr, size), expected, desc);
  }
}

function test_eval_calc_expression() {
  const size = 1000;
  const tests = [{
    desc: "evalCalcExpression with one value",
    expr: "50%",
    expected: 50
  }, {
    desc: "evalCalcExpression with percent and px values",
    expr: "50% + 100px",
    expected: 60
  }, {
    desc: "evalCalcExpression with a zero value",
    expr: "0 + 100px",
    expected: 10
  }, {
    desc: "evalCalcExpression with a negative value",
    expr: "-200px+50%",
    expected: 30
  }];

  for (let { desc, expr, expected } of tests) {
    equal(evalCalcExpression(expr, size), expected, desc);
  }
}

function test_shape_mode_to_css_property_name() {
  const tests = [{
    desc: "shapeModeToCssPropertyName for clip-path",
    expr: "cssClipPath",
    expected: "clipPath"
  }, {
    desc: "shapeModeToCssPropertyName for shape-outside",
    expr: "cssShapeOutside",
    expected: "shapeOutside"
  }];

  for (let { desc, expr, expected } of tests) {
    equal(shapeModeToCssPropertyName(expr), expected, desc);
  }
}

function test_get_circle_path() {
  const tests = [{
    desc: "getCirclePath with size 5, no resizing, no zoom, 1:1 ratio",
    size: 5, cx: 0, cy: 0, width: 100, height: 100, zoom: 1,
    expected: "M-5,0a5,5 0 1,0 10,0a5,5 0 1,0 -10,0"
  }, {
    desc: "getCirclePath with size 7, resizing, no zoom, 1:1 ratio",
    size: 7, cx: 0, cy: 0, width: 200, height: 200, zoom: 1,
    expected: "M-3.5,0a3.5,3.5 0 1,0 7,0a3.5,3.5 0 1,0 -7,0"
  }, {
    desc: "getCirclePath with size 5, resizing, zoom, 1:1 ratio",
    size: 5, cx: 0, cy: 0, width: 200, height: 200, zoom: 2,
    expected: "M-1.25,0a1.25,1.25 0 1,0 2.5,0a1.25,1.25 0 1,0 -2.5,0"
  }, {
    desc: "getCirclePath with size 5, resizing, zoom, non-square ratio",
    size: 5, cx: 0, cy: 0, width: 100, height: 200, zoom: 2,
    expected: "M-2.5,0a2.5,1.25 0 1,0 5,0a2.5,1.25 0 1,0 -5,0"
  }];

  for (let { desc, size, cx, cy, width, height, zoom, expected } of tests) {
    equal(getCirclePath(size, cx, cy, width, height, zoom), expected, desc);
  }
}

function test_get_unit() {
  const tests = [{
    desc: "getUnit with %",
    expr: "30%", expected: "%"
  }, {
    desc: "getUnit with px",
    expr: "400px", expected: "px"
  }, {
    desc: "getUnit with em",
    expr: "4em", expected: "em"
  }, {
    desc: "getUnit with 0",
    expr: "0", expected: "px"
  }, {
    desc: "getUnit with 0%",
    expr: "0%", expected: "px"
  }, {
    desc: "getUnit with no unit",
    expr: "30", expected: "px"
  }, {
    desc: "getUnit with calc",
    expr: "calc(30px + 5%)", expected: "px"
  }, {
    desc: "getUnit with var",
    expr: "var(--variable)", expected: "px"
  }, {
    desc: "getUnit with closest-side",
    expr: "closest-side", expected: "px"
  }, {
    desc: "getUnit with farthest-side",
    expr: "farthest-side", expected: "px"
  }];

  for (let { desc, expr, expected } of tests) {
    equal(getUnit(expr), expected, desc);
  }
}

function test_scale_point() {
  const tests = [{
    desc: "scalePoint with 0,0",
    x: 0, y: 0, transX: 0, transY: 0, scale: 0.9, expected: [0, 0]
  }, {
    desc: "scalePoint with scale factor 1",
    x: 10, y: 10, transX: 100, transY: 100, scale: 1, expected: [10, 10]
  }, {
    desc: "scalePoint with scale factor 0.9, no translation",
    x: 10, y: 20, transX: 0, transY: 0, scale: 0.9, expected: [9, 18]
  }, {
    desc: "scalePoint with scale factor 0.9, translation",
    x: 10, y: 20, transX: 10, transY: 10, scale: 0.9, expected: [10, 19]
  }, {
    desc: "scalePoint with scale factor 2, negative translation",
    x: 20, y: 30, transX: -10, transY: -10, scale: 2, expected: [50, 70]
  }, {
    desc: "scalePoint with scale factor 2, translation = coordinates",
    x: 20, y: 30, transX: 20, transY: 30, scale: 2, expected: [20, 30]
  }];

  for (let { desc, x, y, transX, transY, scale, expected } of tests) {
    let [newX, newY] = scalePoint(x, y, transX, transY, scale);
    equal(newX, expected[0], desc + " x");
    equal(newY, expected[1], desc + " y");
  }
}
