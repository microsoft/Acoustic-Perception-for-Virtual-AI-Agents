# Acoustic Perception for Virtual AI Agents

This repo contains code based on Microsoft Project Acoustics, and implements acoustic perception algorithm for Virtual AI agents described in [this paper](https://dl.acm.org/doi/10.1145/3480139). Below is a demonstration of our method.

[![Demo Video](https://img.youtube.com/vi/Z_t37500Cpg/0.jpg)](https://www.youtube.com/watch?v=Z_t37500Cpg)

The method is wholly contained in the AcousticsSecondaryListener class, split between [the header](Plugins\ProjectAcoustics\Source\ProjectAcoustics\Public\AcousticsSecondaryListener.h) and [implementation](Plugins\ProjectAcoustics\Source\ProjectAcoustics\Private\AcousticsSecondaryListener.cpp).

## Required tools

This project depends on Visual Studio 2019, UE 4.25, and Wwise 2021.1. To run the project, you must:
- Clone the repo. This repo uses [Git LFS](https://git-lfs.github.com/). Ensure Git LFS is installed prior to cloning.
- Integrate Wwise 2021.1 into the project
- Run [PatchWwise.bat](Plugins/ProjectAcoustics/Resources/PatchWwise.bat)
- Generate VS project files for the project
- Build and run from VS.

## Troubleshooting

If you have any issues with the project, please ask questions via the issues tab.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft 
trademarks or logos is subject to and must follow 
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.

## Notice

Digital content in this repository is for demonstration purposes only. The content is directly accessible from the [Unreal Engine Marketplace](https://www.unrealengine.com/marketplace/en-US/store) and is subject to those terms.