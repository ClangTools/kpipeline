cc_binary(
    name = "example",
    srcs = ["main.cc"],
    args = ["$(location :pipeline.json)"],
    data = ["pipeline.json"],
    deps = [
        "@kpipeline//:kpipeline-builder",
    ],
)

cc_binary(
    name = "image",
    srcs = ["main.image.cc"],
    args = ["$(location :image_pipeline.json)"],
    # 更新数据依赖项为新的配置文件
    data = ["image_pipeline.json"],
    deps = [
        "@kpipeline//:kpipeline-builder",
    ],
)

cc_binary(
    name = "conditional",
    srcs = ["main.conditional.cc"],
    args = ["$(location :conditional_pipeline.json)"],
    # 更新数据依赖项为新的配置文件
    data = ["conditional_pipeline.json"],
    deps = [
        "@kpipeline//:kpipeline-builder",
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
        "@kpipeline//:kpipeline-builder",
    ],
)

cc_binary(
    name = "multiple",
    srcs = ["main.multiple.cc"],
    deps = [
        "@kpipeline//:kpipeline-builder",
    ],
)
