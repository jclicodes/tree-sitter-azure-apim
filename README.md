# tree-sitter-azure-apim

Tree-sitter grammar for Azure API Management policy documents.

APIM policies are XML, but the gateway finds policy expressions (`@(...)` and
`@{...}`) by balancing brackets *before* XML parsing. As a result, real policies
contain raw `"`, `<` and `&` inside attribute values:

```xml
<when condition="@(context.Variables.GetValueOrDefault<string>("mode") == "x")">
```

A stock XML grammar rejects this. This grammar lexes expression bodies with an
external scanner (`src/scanner.c`) that tracks bracket depth and skips C# string,
verbatim, interpolated and char literals and comments. The scanner exposes the
body as a `csharp_code` node, so editors can inject a C# grammar into it.

Other nodes: `element`, `start_tag`/`end_tag`/`self_closing_tag`, `attribute`,
`named_value` (`{{name}}`, also Liquid output tags), `entity`, `text`, `comment`,
`cdata` and `processing_instruction`.

```sh
npx tree-sitter generate && npx tree-sitter test
```
