

# C++ AMP

**Accelerated Massive Parallelism with Microsoft Visual C++**  
[Kate Gregory](http://www.gregcons.com/Kate.aspx) and [Ade Miller](http://ademiller.com/tech)

![cover_med.png](http://download-codeplex.sec.s-msft.com/Download?ProjectName=ampbook&DownloadId=488601 "cover_med.jpg")

Capitalize on the faster GPU processors in today’s computers with the C++ AMP code library—and bring massive parallelism to your project. With this practical book, experienced C++ developers will learn parallel programming fundamentals with C++ AMP through detailed examples, code snippets, and case studies. Learn the advantages of parallelism and get best practices for harnessing this library in your applications.  

Discover how to:

*   Gain greater code performance using graphics processing units (GPUs)
*   Choose accelerators that enable you to write code for GPUs
*   Apply thread tiles, tile barriers, and tile static memory
*   Debug C++ AMP code with Microsoft Visual Studio®
*   Use profiling tools to track the performance of your code

Kate Gregory maintains a the [book's homepage](http://www.gregcons.com/cppamp/) which contains updates, links and news of speaking engagements.

### Get the Book

[![books](http://download-codeplex.sec.s-msft.com/Download?ProjectName=ampbook&DownloadId=500805 "books")](http://download-codeplex.sec.s-msft.com/Download?ProjectName=ampbook&DownloadId=500804)The book is now available for purchase, online and in good bookstores! You can read preview material on both the [ Amazon.com](http://www.amazon.com/gp/product/0735664730/ref=as_li_ss_tl?ie=UTF8&tag=alpineclimbin-20&linkCode=as2&camp=1789&creative=390957&creativeASIN=0735664730) (paper and Kindle versions) and the [ O’Reilly web site](http://shop.oreilly.com/product/0790145341907.do) (DRM free eBook, PDF and paper versions). You can also read it  through [ Safari Books online](http://my.safaribooksonline.com/book/programming/cplusplus/9780735668171). The list prices; print $36.99, eBook $29.99, both $40.69.

If you like the book and want to write a review of it then [Amazon.com](http://www.amazon.com/gp/product/0735664730/ref=as_li_ss_tl?ie=UTF8&tag=alpineclimbin-20&linkCode=as2&camp=1789&creative=390957&creativeASIN=0735664730) is the place to do that.

### Download the case studies and sample code for each chapter

![N-body model on a single GPU.](http://download-codeplex.sec.s-msft.com/Download?ProjectName=ampbook&DownloadId=477775 "N-body model on a single GPU.")The **N-body** case study shows how to use C++ AMP to get the most out of your GPU hardware in a computational application. It contains several implementations of the classic n-body problem that models particles moving under the influence of gravity. The code has implementations for simple and tiled C++ AMP kernels as well as an implementation that runs on more than one GPU. The accompanying CPU based sample also includes single and multi-core implementations of the same algorithm. The case study also shows the use of inter-op with DirectX to minimize the overhead of displaying your application’s results.

This N-body application is similar to the one [written by the C++ AMP product team](http://blogs.msdn.com/b/nativeconcurrency/archive/2011/09/20/c-amp-n-body-simulation-sample.aspx), it was originally based on the same source code. However, it includes several improvements and new features:

*   Updated code to use more of the modern C++11 programming idioms.
*   Added additional GPU implementations for multi-GPU.
*   An additional CPU application with SSE2 enabled implementations and a different algorithm that makes better use of the CPU's cache architecture. This allows fairer comparisons between CPU and GPU implementations.

![Cartoonizer in action.](http://download-codeplex.sec.s-msft.com/Download?ProjectName=ampbook&DownloadId=427420 "Cartoonizer in action.")The **Cartoonizer** case study demonstrates braided parallelism, using both the available cores on the CPU and any available GPU(s). It implements  color simplification and edge detection algorithms using C++ AMP and orchestrates the processing of images using the [ Parallel Patterns Library](http://msdn.microsoft.com/en-us/library/dd492418.aspx) and [ Asynchronous Agents Library](http://msdn.microsoft.com/en-us/library/dd492627.aspx). Single accelerator implementations of simple, tiled and texture based algorithms are all shown. In addition, the case study also shows two approaches for dividing the cartoonizing workload up across more than one accelerator, either by splitting images into subsections or forking the pipeline and processing images on separate accelerators before multiplexing them back into the correct sequence.

![Profiling the reduce algorithm.](http://download-codeplex.sec.s-msft.com/Download?ProjectName=ampbook&DownloadId=427422 "Profiling the reduce algorithm.")

The **Reduction** case study shows twelve different implementations of the [reduce algorithm](http://en.wikipedia.org/wiki/Reduce_(higher-order_function)). Each implementation shows different approaches and the book discusses their performance characteristics and the trade-offs associated with each implementation. Reduction is an important data parallel operation so it is worth considering its implementation in some detail.

Finally all the **code samples** associated with the other chapters in the book can also be found here.

**System Requirements**

We recommend [Visual Studio 2012 or 2013 Professional](http://www.microsoft.com/visualstudio/11/en-us/downloads) to run the samples. The [DirectX SDK (June 2010)](http://www.microsoft.com/en-us/download/details.aspx?id=6812) is also required on both Windows 7 & 8 to build the N-body case study and Chapter 11 samples. If the SDK fails to install with error code S1023 you have a more recent version of the VS 2010 redistributable installed. Uninstall the redistributable, then reinstall the SDK to resolve this issue. [More details can be found here](http://blogs.msdn.com/b/chuckw/archive/2011/12/09/known-issue-directx-sdk-june-2010-setup-and-the-s1023-error.aspx).

You can also use [Visual Studio Express 2012 or 2013 for Windows Desktop](http://www.microsoft.com/visualstudio/eng/downloads#d-express-windows-desktop) to build and run <span style="text-decoration:underline">some</span> of the sample projects and case studies. The sample projects are classic style native applications so will not load in Visual Studio Express 2012 for Windows 8. VS Express does [not support ATL or MFC](http://msdn.microsoft.com/en-us/library/hh967573.aspx) so the samples and case studies that use MFC will not build; NBody and Cartoonizer.

**Visualization Tools:** You will need [Visual Studio 2012 Ultimate](http://www.microsoft.com/visualstudio/11/en-us/downloads) to use some the parallel diagnostic tools such as the Concurrency Visualizer. You may also have to enable it for the Reduction project. Select the Analyze | Concurrency Visualizer | Add SDK to Project... menu item, then select the Reduction project and click Add SDK to Selected Project.

**Debugger and WARP accelerator support:** Debugging and WARP accelerator support is now available on Windows 7 with the [Platform Update for Windows 7](http://www.microsoft.com/en-us/download/details.aspx?id=36805). In addition NVidia now supports hardware debugging. The following blog posts outline how to use these:

*   [C++ AMP CPU fallback support now available on Windows 7](http://blogs.msdn.com/b/nativeconcurrency/archive/2013/03/26/c-amp-cpu-fallback-support-now-available-in-windows-7.aspx)
*   [C++ AMP GPU debugging now available on Windows 7](http://blogs.msdn.com/b/nativeconcurrency/archive/2013/01/25/c-amp-gpu-debugging-now-available-on-windows-7.aspx)
*   [Remote GPU Debugging on NVidia Hardware](http://blogs.msdn.com/b/nativeconcurrency/archive/2013/02/06/remote-gpu-debugging-on-nvidia-hardware.aspx)

**Additional Requirements for Visual Studio 2013**

The existing Visual Studio 2012 projects and solution will automatically upgrade when opened in Visual Studio 2013\. Simply accept the upgrade dialog and the solution will load.

**Enabling Code Markers:** If you are using Visual Studio 2013 and want to use the code markers feature in the Reduction case study then you will need to download and install the [Concurrency Visualizer extension](http://visualstudiogallery.msdn.microsoft.com/24b56e51-fcc2-423f-b811-f16f3fa3af7a). The visualizer no longer ships as part of Visual Studio. Once you have installed the add-in enable it for the Reduction project. Select the Analyze | Concurrency Visualizer | Add SKD to Project... menu item, then select the Reduction project and click "Add SDK to Selected Project".

If you do not want to use this feature the simply remove the MARKERS macro definition at the top of the Reduction.cpp source file before compiling.

### Demos and Talks

Ade Miller gave a talk at the [NVidia GPU Technology Conference](http://www.gputechconf.com/) you can find the slide deck for this talk on [his blog](http://www.ademiller.com/blogs/tech/2013/03/gpu-technology-conference-2013/). If you attended GTC and missed the talk then a video of it is available on the [GTC web site](http://www.gputechconf.com/).

*   S3317 - An Overview of Accelerated Parallelism with C++ AMP

The source code for the examples in the talk are checked in to the Extras folder in the source tree.

Don McCrady (Development Lead for C++ AMP) and Jim Radigan (Architect on Windows C++) gave an excellent talk on performance programming with C++ and C++ AMP at [//BUILD 2012](http://www.buildwindows.com/). The C++ AMP material is towards the end.

*   [It’s all about performance: Using Visual C++ 2012 to make the best use of your hardware](http://channel9.msdn.com/Events/Build/2012/3-013)

Kate Gregory spoke at TechEd 2012\. You can watch the talk here:

*   [C++ Accelerated Massive Parallelism in Visual C++ 2012](http://channel9.msdn.com/Events/TechEd/Europe/2012/DEV334)

Daniel Moth gave two talks on C++ AMP at the GPU Technology Conference 2012\. This included a showing the Cartoonizer case study.

*   [Harnessing GPU Compute with C++ AMP (Part 1 of 2)](http://nvidia.fullviewmedia.com/gtc2012/0516-A3-S0242.html)
*   [Harnessing GPU Compute with C++ AMP (Part 2 of 2)](http://nvidia.fullviewmedia.com/gtc2012/0517-C-S0244.html)

There is also a shorter Channel 9 video, also by Daniel, that walks through the Cartoonizer application:

*   [Cartoonizer - C++ AMP sample](http://channel9.msdn.com/Blogs/DanielMoth/Cartoonizer-C-AMP-sample)

The C++ AMP team blogs frequently on the [Parallel Programming in Native Code](http://blogs.msdn.com/b/nativeconcurrency/) blog. Check there for new updates about C++ AMP and parallel programming in general. They also have a collection of samples that are very helpful for understanding how to implement solutions to specific problems in C++ AMP:

*   [C++ AMP sample projects for download](http://blogs.msdn.com/b/nativeconcurrency/archive/2012/01/30/c-amp-sample-projects-for-download.aspx)

The [Parallel Computing in C++ and Native Code](http://social.msdn.microsoft.com/Forums/en-US/parallelcppnative/threads) forum on MSDN is good place to ask questions about C++ AMP.

