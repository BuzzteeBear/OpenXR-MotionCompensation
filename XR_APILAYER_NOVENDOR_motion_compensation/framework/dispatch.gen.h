// *********** THIS FILE IS GENERATED - DO NOT EDIT ***********
// MIT License
//
// Copyright(c) 2021-2022 Matthieu Bucchianeri
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this softwareand associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and /or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright noticeand this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#pragma once

#ifndef LAYER_NAMESPACE
#error Must define LAYER_NAMESPACE
#endif

namespace LAYER_NAMESPACE
{

	class OpenXrApi
	{
	private:
		XrInstance m_instance{ XR_NULL_HANDLE };
		std::string m_applicationName;

	protected:
		OpenXrApi() = default;

		PFN_xrGetInstanceProcAddr m_xrGetInstanceProcAddr{ nullptr };
        
        std::set<std::string> m_extensions;

	public:
		virtual ~OpenXrApi() = default;

		XrInstance GetXrInstance() const
		{
			return m_instance;
		}

		const std::string& GetApplicationName() const
		{
			return m_applicationName;
		}

		void SetGetInstanceProcAddr(PFN_xrGetInstanceProcAddr pfn_xrGetInstanceProcAddr, XrInstance instance)
		{
			m_xrGetInstanceProcAddr = pfn_xrGetInstanceProcAddr;
			m_instance = instance;
		}

        void SetExtensions(const std::vector<std::string>& extensions)
        {
            m_extensions = std::set(extensions.begin(), extensions.end());
        }

        bool IsExtensionActivated(const std::string extensionName)
        {
            return (bool)m_extensions.count(extensionName);
        }

		// Specially-handled by the auto-generated code.
		virtual XrResult xrGetInstanceProcAddr(XrInstance instance, const char* name, PFN_xrVoidFunction* function);
		virtual XrResult xrCreateInstance(const XrInstanceCreateInfo* createInfo);


		// Auto-generated entries for the requested APIs.

	public:
		virtual XrResult xrDestroyInstance(XrInstance instance)
		{
			return m_xrDestroyInstance(instance);
		}
	private:
		PFN_xrDestroyInstance m_xrDestroyInstance{ nullptr };

	public:
		virtual XrResult xrGetInstanceProperties(XrInstance instance, XrInstanceProperties* instanceProperties)
		{
			return m_xrGetInstanceProperties(instance, instanceProperties);
		}
	private:
		PFN_xrGetInstanceProperties m_xrGetInstanceProperties{ nullptr };

	public:
		virtual XrResult xrGetSystem(XrInstance instance, const XrSystemGetInfo* getInfo, XrSystemId* systemId)
		{
			return m_xrGetSystem(instance, getInfo, systemId);
		}
	private:
		PFN_xrGetSystem m_xrGetSystem{ nullptr };

	public:
		virtual XrResult xrGetSystemProperties(XrInstance instance, XrSystemId systemId, XrSystemProperties* properties)
		{
			return m_xrGetSystemProperties(instance, systemId, properties);
		}
	private:
		PFN_xrGetSystemProperties m_xrGetSystemProperties{ nullptr };

	public:
		virtual XrResult xrCreateSession(XrInstance instance, const XrSessionCreateInfo* createInfo, XrSession* session)
		{
			return m_xrCreateSession(instance, createInfo, session);
		}
	private:
		PFN_xrCreateSession m_xrCreateSession{ nullptr };

	public:
		virtual XrResult xrCreateReferenceSpace(XrSession session, const XrReferenceSpaceCreateInfo* createInfo, XrSpace* space)
		{
			return m_xrCreateReferenceSpace(session, createInfo, space);
		}
	private:
		PFN_xrCreateReferenceSpace m_xrCreateReferenceSpace{ nullptr };

	public:
		virtual XrResult xrCreateActionSpace(XrSession session, const XrActionSpaceCreateInfo* createInfo, XrSpace* space)
		{
			return m_xrCreateActionSpace(session, createInfo, space);
		}
	private:
		PFN_xrCreateActionSpace m_xrCreateActionSpace{ nullptr };

	public:
		virtual XrResult xrLocateSpace(XrSpace space, XrSpace baseSpace, XrTime time, XrSpaceLocation* location)
		{
			return m_xrLocateSpace(space, baseSpace, time, location);
		}
	private:
		PFN_xrLocateSpace m_xrLocateSpace{ nullptr };

	public:
		virtual XrResult xrDestroySpace(XrSpace space)
		{
			return m_xrDestroySpace(space);
		}
	private:
		PFN_xrDestroySpace m_xrDestroySpace{ nullptr };

	public:
		virtual XrResult xrLocateViews(XrSession session, const XrViewLocateInfo* viewLocateInfo, XrViewState* viewState, uint32_t viewCapacityInput, uint32_t* viewCountOutput, XrView* views)
		{
			return m_xrLocateViews(session, viewLocateInfo, viewState, viewCapacityInput, viewCountOutput, views);
		}
	private:
		PFN_xrLocateViews m_xrLocateViews{ nullptr };

	public:
		virtual XrResult xrStringToPath(XrInstance instance, const char* pathString, XrPath* path)
		{
			return m_xrStringToPath(instance, pathString, path);
		}
	private:
		PFN_xrStringToPath m_xrStringToPath{ nullptr };

	public:
		virtual XrResult xrPathToString(XrInstance instance, XrPath path, uint32_t bufferCapacityInput, uint32_t* bufferCountOutput, char* buffer)
		{
			return m_xrPathToString(instance, path, bufferCapacityInput, bufferCountOutput, buffer);
		}
	private:
		PFN_xrPathToString m_xrPathToString{ nullptr };

	public:
		virtual XrResult xrCreateActionSet(XrInstance instance, const XrActionSetCreateInfo* createInfo, XrActionSet* actionSet)
		{
			return m_xrCreateActionSet(instance, createInfo, actionSet);
		}
	private:
		PFN_xrCreateActionSet m_xrCreateActionSet{ nullptr };

	public:
		virtual XrResult xrDestroyActionSet(XrActionSet actionSet)
		{
			return m_xrDestroyActionSet(actionSet);
		}
	private:
		PFN_xrDestroyActionSet m_xrDestroyActionSet{ nullptr };

	public:
		virtual XrResult xrCreateAction(XrActionSet actionSet, const XrActionCreateInfo* createInfo, XrAction* action)
		{
			return m_xrCreateAction(actionSet, createInfo, action);
		}
	private:
		PFN_xrCreateAction m_xrCreateAction{ nullptr };

	public:
		virtual XrResult xrDestroyAction(XrAction action)
		{
			return m_xrDestroyAction(action);
		}
	private:
		PFN_xrDestroyAction m_xrDestroyAction{ nullptr };

	public:
		virtual XrResult xrSuggestInteractionProfileBindings(XrInstance instance, const XrInteractionProfileSuggestedBinding* suggestedBindings)
		{
			return m_xrSuggestInteractionProfileBindings(instance, suggestedBindings);
		}
	private:
		PFN_xrSuggestInteractionProfileBindings m_xrSuggestInteractionProfileBindings{ nullptr };

	public:
		virtual XrResult xrAttachSessionActionSets(XrSession session, const XrSessionActionSetsAttachInfo* attachInfo)
		{
			return m_xrAttachSessionActionSets(session, attachInfo);
		}
	private:
		PFN_xrAttachSessionActionSets m_xrAttachSessionActionSets{ nullptr };

	public:
		virtual XrResult xrGetActionStatePose(XrSession session, const XrActionStateGetInfo* getInfo, XrActionStatePose* state)
		{
			return m_xrGetActionStatePose(session, getInfo, state);
		}
	private:
		PFN_xrGetActionStatePose m_xrGetActionStatePose{ nullptr };

	public:
		virtual XrResult xrSyncActions(XrSession session, const XrActionsSyncInfo* syncInfo)
		{
			return m_xrSyncActions(session, syncInfo);
		}
	private:
		PFN_xrSyncActions m_xrSyncActions{ nullptr };



	};

} // namespace LAYER_NAMESPACE

