# Add a custom OLE format

Call `RegisterOleFormat` with a versioned stable name. Add copied bytes through
`OleDataObjectBuilder`; define a maximum payload and a self-describing wire
format. Never transfer application pointers. The data object owns copied global
memory and delayed renderers until COM releases it. Request the format with a
matching `FORMATETC` and release every returned `STGMEDIUM`.
