# Lumin
My personal C++ renderer with DirectX 12 API and shaders based on Frank D. Luna's *Introduction to 3D Game Programming with DirectX 12*.
More general information about this book can be found [here](http://www.d3dcoder.net/d3d12.htm).

## Structure
```
├───engine              // Engine framework 
│   ├───engine            // Engine source code
|   |   ├───common          // Common engine code
|   |   ├───gui             // GUI integration
|   |   └───shaders         // HLSL shaders
│   ├───resources         // Resouce files (ex. models, textures, fonts, etc.) 
│   └───external          // Third party libs (ex. ImGui, etc.)
└───projects            // Projects
    ├───LunaDemoScenes    // Two demo scenes provided in Frank D. Luna's tutorial book
    └───ShadowsScene      // Demo scene to experiment with different real-time shadow techniques
```


