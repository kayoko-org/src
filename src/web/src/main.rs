use clap::{Parser, Subcommand};
use gray_matter::engine::YAML;
use gray_matter::Matter;
use pulldown_cmark::{html, Options, Parser as CmarkParser};
use serde::{Deserialize, Serialize};
use std::fs;
use std::path::{Path, PathBuf};
use tera::{Context, Tera};
use walkdir::WalkDir;

/// ----------------------------------------------------------------------------
/// 1. DATA STRUCTURES
/// ----------------------------------------------------------------------------

#[derive(Parser)]
#[command(name = "Yarbs")]
#[command(about = "Yet Another Rust Based SSG", long_about = None)]
struct Cli {
    #[command(subcommand)]
    command: Commands,
}

#[derive(Subcommand)]
enum Commands {
    /// Scaffolds a new Yarbs project structure
    Init,
    /// Compiles the content/ directory into the public/ directory
    Build,
}

#[derive(Serialize, Deserialize, Debug)]
struct PostMeta {
    title: String,
    date: String,
    #[serde(default)]
    description: Option<String>,
}

/// ----------------------------------------------------------------------------
/// 2. CORE LOGIC: PARSING & COMPILING
/// ----------------------------------------------------------------------------

fn compile_markdown(raw_content: &str) -> Result<(PostMeta, String), Box<dyn std::error::Error>> {
    let matter = Matter::<YAML>::new();
    
    // Explicitly type 'parsed' so the compiler knows to use PostMeta for the data field
    let parsed: gray_matter::ParsedEntity<PostMeta> = matter
        .parse(raw_content)
        .map_err(|e| e.to_string())?;
    
    let meta = match parsed.data {
        Some(data) => data, // It's already a PostMeta now!
        None => return Err("Missing frontmatter".into()),
    };

    // Convert Markdown to HTML
    let mut options = Options::empty();
    options.insert(Options::ENABLE_TABLES);
    options.insert(Options::ENABLE_STRIKETHROUGH);
    
    let parser = CmarkParser::new_ext(&parsed.content, options);
    let mut html_output = String::new();
    html::push_html(&mut html_output, parser);

    Ok((meta, html_output))
}

/// ----------------------------------------------------------------------------
/// 3. COMMAND: INIT
/// ----------------------------------------------------------------------------

fn run_init() -> Result<(), Box<dyn std::error::Error>> {
    println!("🏴‍☠️ Initializing Yarbs project...");

    fs::create_dir_all("content")?;
    fs::create_dir_all("templates")?;
    fs::create_dir_all("public")?;

    // Create a default layout template
    let default_template = r#"<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <title>{{ post.title }}</title>
</head>
<body>
    <header>
        <h1>{{ post.title }}</h1>
        <p>Published on: {{ post.date }}</p>
    </header>
    <main>
        {{ content | safe }}
    </main>
</body>
</html>"#;
    fs::write("templates/post.html", default_template)?;

    // Create a sample markdown post
    let sample_post = r#"---
title: "Hello from Yarbs!"
date: "2026-04-03"
description: "My first post using Yet Another Rust Based SSG."
---
## Welcome

This is your first post. **Yarbs** is successfully parsing this Markdown, extracting the YAML frontmatter, and injecting it into your `post.html` template.

Enjoy the speed!
"#;
    fs::write("content/hello-world.md", sample_post)?;

    println!("✅ Done! Run `cargo run -- build` to generate your site.");
    Ok(())
}

/// ----------------------------------------------------------------------------
/// 4. COMMAND: BUILD
/// ----------------------------------------------------------------------------

fn run_build() -> Result<(), Box<dyn std::error::Error>> {
    println!("🔨 Building site...");

    let tera = Tera::new("templates/**/*.html")?;
    let output_dir = "public";
    let static_dir = "static";

    // 1. Clean and recreate public/
    if Path::new(output_dir).exists() {
        fs::remove_dir_all(output_dir)?;
    }
    fs::create_dir_all(output_dir)?;

    // 2. Copy static assets if they exist
    if Path::new(static_dir).exists() {
        println!("  🎨 Copying static assets...");
        copy_dir_all(static_dir, output_dir)?;
    }

    let mut build_count = 0;

    // 3. Process Markdown (Existing Logic)
    for entry in WalkDir::new("content").into_iter().filter_map(|e| e.ok()) {
        let path = entry.path();
        if path.is_file() && path.extension().and_then(|s| s.to_str()) == Some("md") {
            let raw_content = fs::read_to_string(path)?;
            
            let (meta, html_content) = compile_markdown(&raw_content)?;
            let mut context = Context::new();
            context.insert("post", &meta);
            context.insert("content", &html_content);

            let rendered = tera.render("post.html", &context)?;
            let file_stem = path.file_stem().unwrap().to_str().unwrap();
            let dest_path = PathBuf::from(output_dir).join(format!("{}.html", file_stem));
            
            fs::write(dest_path, rendered)?;
            println!("  📄 Generated: {}.html", file_stem);
            build_count += 1;
        }
    }

    println!("🚀 Build complete! Generated {} pages.", build_count);
    Ok(())
}

/// Helper: Recursively copies a directory's contents
fn copy_dir_all(src: impl AsRef<Path>, dst: impl AsRef<Path>) -> std::io::Result<()> {
    fs::create_dir_all(&dst)?;
    for entry in fs::read_dir(src)? {
        let entry = entry?;
        let ty = entry.file_type()?;
        if ty.is_dir() {
            copy_dir_all(entry.path(), dst.as_ref().join(entry.file_name()))?;
        } else {
            fs::copy(entry.path(), dst.as_ref().join(entry.file_name()))?;
        }
    }
    Ok(())
}

/// ----------------------------------------------------------------------------
/// 5. MAIN EXECUTION
/// ----------------------------------------------------------------------------

fn main() {
    let cli = Cli::parse();

    let result = match &cli.command {
        Commands::Init => run_init(),
        Commands::Build => run_build(),
    };

    if let Err(e) = result {
        eprintln!("💥 Fatal Error: {}", e);
        std::process::exit(1);
    }
}
