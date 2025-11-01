# nerdui
Turning data models into interactive experiences

NerdUI is meant to auto-generate user interfaces given a data model in a yaml form.

The data model is rich enough to specify various types of constraints, such as which data member is relevant under what constraint etc.

NerdUI explores the idea of UI as object construction through interactivity. It cleanly separates the modalities of interactivity from the data model.

Initially we will be focusing on a command line interface that works in a terminal and later on explore web interfaces.

The focus of this framework will be on automation so that no code is required to be written to build a UI, completeness, type safety, performance etc and not on cosmetic aspects of the UI.
