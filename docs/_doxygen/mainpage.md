# MCP970x Driver API reference {#mainpage}

The classes and types of the MCP9700, MCP9700A, MCP9700B, MCP9701 and MCP9701A driver, generated from the headers in
`inc/`.

Start with mcp9700::MCP9700, the driver, and mcp9700::AdcInterface, the base of the port you write. Errors are
mcp9700::Error inside mcp9700::Result; the configuration is mcp9700::Config, and the five devices are rows of
mcp9700::kVariantTable.

The guides, with examples that compile, are on the documentation site: <a href="../">MCP970x Driver
documentation</a>.
