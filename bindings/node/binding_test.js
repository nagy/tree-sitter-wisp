const assert = require("node:assert");
const { describe, it, beforeEach } = require("node:test");
const Parser = require("tree-sitter");
const Wisp = require(".");

describe("wisp grammar", () => {
  let parser;

  beforeEach(() => {
    parser = new Parser();
    parser.setLanguage(Wisp);
  });

  it("parses a factorial", () => {
    const tree = parser.parse(`
define : factorial n
  if : zero? n
    . 1
    * n : factorial {n - 1}
`);
    assert.strictEqual(tree.rootNode.hasError, false);
    assert.ok(tree.rootNode.toString().includes("(colon_list"));
  });
});
