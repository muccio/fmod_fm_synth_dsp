studio.plugins.registerPluginDescription("FM Synthesizer", {
    companyName: "Custom Audio",
    productName: "FM Synthesizer",
    parameters: {
        "Carrier Freq": {
            displayName: "Carrier Freq"
        },
        "Mod Ratio": {
            displayName: "Mod Ratio"
        },
        "Mod Index": {
            displayName: "Mod Index"
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
                binding: "Carrier Freq"
            },
            {
                deckWidgetType: studio.ui.deckWidgetType.Dial,
                binding: "Mod Ratio"
            },
            {
                deckWidgetType: studio.ui.deckWidgetType.Dial,
                binding: "Mod Index"
            },
            {
                deckWidgetType: studio.ui.deckWidgetType.Fader,
                binding: "Volume"
            }
        ]
    }
});
