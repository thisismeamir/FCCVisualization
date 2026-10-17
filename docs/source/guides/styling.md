# Styling recipes

Styles are rules: a style, optionally with a condition. Rules are applied in
order and later rules override the properties they set; properties no rule
sets come from the backend's defaults (the figure theme). See
{doc}`../concepts/styles` and `help styles`.

## Rules and conditions

```{literalinclude} ../../examples/guides/styling.fcv
:language: text
:start-after: "# [rules]"
:end-before: "# [rules]"
```

All hits are small and grey; hits above 2 GeV are bigger and in the accent
colour; hits of the HCal barrel are squares whatever their energy (the two
conditional rules set different properties, so both apply).

## Presets

```{literalinclude} ../../examples/guides/styling.fcv
:language: text
:start-after: "# [presets]"
:end-before: "# [presets]"
```

A preset adds its rules like any other `style` command: rules after it can
refine them. Presets: `hits-by-energy`, `tracks-by-charge`,
`detector-outline`, `ghosted`, `color-by <attribute> lo hi`, `size-by ...`.

## The scene environment

```{literalinclude} ../../examples/guides/styling.fcv
:language: text
:start-after: "# [environment]"
:end-before: "# [environment]"
```

`scene show` lists the rules in order:

```{literalinclude} ../../examples/guides/styling.out
:language: text
:start-after: "unit:pt]"
:end-before: "--- stderr"
```

Backends declare the style properties they support (`help backend <type>`);
a property a backend does not support is ignored by it, so the same scene
can be sent to backends with different abilities.
