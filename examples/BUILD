cc_binary(
    name = "example",
    srcs = ["main.cc"],
    data = ["pipeline.json"],
    deps = [
        "@kpipeline",
    ],
)

cc_binary(
    name = "image",
    srcs = ["main.image.cc"],
    # 更新数据依赖项为新的配置文件
    data = ["image_pipeline.json"],
    deps = [
        "@kpipeline",
    ],
)

cc_binary(
    name = "conditional",
    srcs = ["main.conditional.cc"],
    # 更新数据依赖项为新的配置文件
    data = ["conditional_pipeline.json"],
    deps = [
        "@kpipeline",
    ],
)

cc_binary(
    name = "subgraph",
    srcs = ["main.sub.graph.node.cc"],
    data = [
        "main_pipeline.json",
        "photo_analysis_subgraph.json",
    ],
    deps = [
        "@kpipeline",
    ],
)

cc_binary(
    name = "multiple",
    srcs = ["main.multiple.cc"],
    deps = [
        "@kpipeline",
    ],
)
