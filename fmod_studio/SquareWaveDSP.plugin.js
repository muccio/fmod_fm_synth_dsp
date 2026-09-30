studio.plugins.registerPluginDescription("Square Wave Generator", {
    companyName: "Custom Audio",
    productName: "Square Wave Generator",
    parameters: {
        "Frequency": {
            displayName: "Frequency"
        },
        "Volume": {
            displayName: "Volume"
        }
    },
    deckUi: {
        deckWidgetType: studio.ui.deckWidgetType.Layout,
        layout: studio.ui.layoutType.HBoxLayout,
        spacing: 16,
        contentsMargins: { left: 12, top: 8, right: 12, bottom: 8 },
        items: [
            {
                deckWidgetType: studio.ui.deckWidgetType.Dial,
                binding: "Frequency"
            },
            {
                deckWidgetType: studio.ui.deckWidgetType.Fader,
                binding: "Volume"
            }
        ]
    }
});
